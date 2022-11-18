CRPS - Cereal Raw Pointer Support (Experimental)
==========================================

CRPS enables serialization of raw pointers which point to objects that are co-serialized by the graph traversal. This allows for references to be taken to a struct's members without allocating the members seperately with a std::shared_ptr. CRPS was intented for cases where a class is allocated with a std::shared_ptr, but a reference to one it's members must persist as well. 
<br></br>

## Usage

To use CRPS, construct an archive as normal, and then place the archive into the constructor of either ```crps::CRPSOutputArchive<ArchiveType>``` or ```crps::CRPSInputArchive<ArchiveType>```. All serialization calls must be done through the CRPSArchive class instead of the ArchiveType archive. To finish serialization, the CRPSArchive's destructor must execute, or alternatively the CRPSArchive complete() method may be called. 

A class may serialize a ```T*``` by passing it into ```crps::make_raw_ptr(T*)``` before the archive call. Alternatively, ```crps::raw_ptr<T>``` may be used in place of ```T*```. For STL types, ```std::vector<T*>``` does not compile, but ```std::vector<raw_ptr<T>>``` does.
<br></br>

**Example 1:** 

```cpp
#include <cereal/archives/binary.hpp>
#include <crps/crps.hpp>
#include <fstream>

int main() {

    int x = 4;
    int* y = &x;
    {
        std::ofstream os("save.out", std::ios::binary);
        cereal::BinaryOutputArchive oarchive(os);
        crps::CRPSOutputArchive<cereal::BinaryOutputArchive> crps_oarchive(oarchive);

        crps_oarchive(x, crps::make_raw_ptr(y));
    }

    int x_load;
    int* y_load;
    {
        std::ifstream is("save.out", std::ios::binary);
        cereal::BinaryInputArchive iarchive(is);
        crps::CRPSInputArchive<cereal::BinaryInputArchive> crps_iarchive(iarchive);

        crps_iarchive(x_load, crps::make_raw_ptr(y_load));
    }

    std::cout << *y_load << std::endl; //4

}
```    
<br></br>
**Example 2:** In this example, instead of a graph with vertex-to-vertex connections, a vertex connects to a target within another vertex. Therefore, the edge class cannot just store two shared_ptrs without also keeping track of a local target index within the second vertex. With CRPS, the Edge class can store a raw pointer to the target within the second vertex.

```cpp
struct Vertex {

    float targetA;
    float targetB;

    template<class Archive>
    void serialize(Archive& ar)
    { ar(targetA, targetB); }

};

struct Edge {
    
    std::shared_ptr<Vertex> vertex;
    crps::raw_ptr<float> target; //can either point to a vertex's targetA or targetB

    template<class Archive>
    void serialize(Archive& ar)
    { ar(vertex, target); }

};

int main() {

    std::shared_ptr<Vertex> v1 = std::make_shared<Vertex>(Vertex{ 1, 2 });
    std::shared_ptr<Vertex> v2 = std::make_shared<Vertex>(Vertex{ 4, 5 });

    Edge e1{ v1, &v1->targetB }; //v1 connects to it's own targetB
    Edge e2{ v1, &v2->targetA }; //v1 connects to v2's targetA


    std::vector<std::shared_ptr<Vertex>> vertices{ v1, v2 };
    std::vector<Edge> edges{ e1, e2 };
    {
        std::ofstream os("out.txt", std::ios::binary);
        cereal::BinaryOutputArchive oarchive(os);
        crps::CRPSOutputArchive<cereal::BinaryOutputArchive> crps_oarchive(oarchive);

        crps_oarchive(vertices, edges);
    }


    std::vector<std::shared_ptr<Vertex>> vertices_load;
    std::vector<Edge> edges_load;
    {
        std::ifstream is("out.txt", std::ios::binary);
        cereal::BinaryInputArchive iarchive(is);
        crps::CRPSInputArchive<cereal::BinaryInputArchive> crps_iarchive(iarchive);

        crps_iarchive(vertices_load, edges_load);
    }

    std::cout << *edges_load[0].target << std::endl; //2
    std::cout << *edges_load[1].target << std::endl; //4

}
```
<br></br>

## this-pointer support

CRPS allows for serialization of pointers to class types which are co-serialized by the graph traversal.
To do this, the class type must pass a ```crps::this_ptr(this)``` into the archive of its member serialize method. 

**Example 3:**

```cpp
struct Point {
    
    float x, y;
    template<class Archive>
    void serialize(Archive& ar)
    {
        ar(x, y, crps::this_ptr(this));
    }
};

int main() {

    {
        Point point{ 3, 4 };
        Point* p = &point;
        float* x = &point.x;
        float* y = &point.y;

        std::ofstream os("save.out", std::ios::binary);
        cereal::BinaryOutputArchive oarchive(os);
        crps::CRPSOutputArchive<cereal::BinaryOutputArchive> crps_oarchive(oarchive);

        crps_oarchive(crps::make_raw_ptr(p), crps::make_raw_ptr(x), crps::make_raw_ptr(y), point);
    }


    Point point;
    Point* p;
    float* x;
    float* y;
    {
        std::ifstream is("save.out", std::ios::binary);
        cereal::BinaryInputArchive iarchive(is);
        crps::CRPSInputArchive<cereal::BinaryInputArchive> crps_iarchive(iarchive);

        crps_iarchive(crps::make_raw_ptr(p), crps::make_raw_ptr(x), crps::make_raw_ptr(y), point);
    }

    std::cout << &point << std::endl; 
    std::cout << p << std::endl; // prints &point
    std::cout << x << std::endl; // prints &point
    std::cout << y << std::endl; // prints &point + sizeof(float) bytes
    
}
```
<br></br>

## Binary Data

CRPS does not currently support tracking pointers during the serialization of binary data. 
