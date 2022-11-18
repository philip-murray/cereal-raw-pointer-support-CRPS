#ifndef CRPS_HPP_
#define CRPS_HPP_

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include <sstream>

namespace crps 
{
    // ######################################################################
    //! Runtime error class for CRPS
    /*! @ingroup Utility */
    struct CRPSException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    namespace detail 
    {
        class CRPSMapperCore {}; //!< Traits struct for CRPSOutputMapper and CRPSInputMapper
    }

    template <typename T>
    using PtrWrapper = cereal::memory_detail::PtrWrapper<T>; //for CSFN specializations

    // ######################################################################
    //! An archivable wrapper for raw pointers
    /*! An archivable wrapper for raw pointers to types co-serialized in the 
        graph traversal. This class allows for raw pointers to be passed to the 
        CRPSMapper without triggering the static_assert in the CSFN specialization 
        for raw pointers. Provides CRPSMapper with an l-value reference to a T*. 

        @internal */
    template<class T>
    class RawPointer
    {
    public:

        RawPointer(T*& ptr) : ptr(ptr) {}

        T*& ptr;

        //! Register the raw pointer with CRPSOutputMapper or CRPSInputMapper
        template<class Archive> inline
        typename std::enable_if<std::is_base_of<detail::CRPSMapperCore, Archive>::value, void>::type
        CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar)
        {
            CEREAL_SAVE_FUNCTION_NAME(ar, cereal::memory_detail::PtrWrapper<T*&>(ptr));
        }

        //! Representation for the user archive. 
        template<class Archive> inline
        typename std::enable_if<!std::is_base_of<detail::CRPSMapperCore, Archive>::value, void>::type
        CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar)
        {
            
        }
    };

    //! Creates a RawPointer that stores an l-value reference to a T*
    /*! Creates a RawPointer that stores an l-value reference to a T*
        @relates RawPointer
        @ingroup Utility 
        */
    template <class T> inline
    RawPointer<T> make_raw_ptr(T*& ptr)
    {
        return { ptr };
    }

    // ######################################################################
    //! An archivable wrapper that stores a reference to a class type.
    /*! This class allows the CRPSMapper to intercept and register the memory 
        address of classes that are visited by the graph traversal. Provides 
        an l-value reference the class type. 

        The default cereal::OutputArchive::processImpl method for class types 
        works by making a recursive archive call to their members, so the derived 
        archive implemenation is not otherwise able to take the memory address of 
        a class. 
        @internal */
    template<class T>
    class ThisPointer
    {
    public:

        ThisPointer(T* ptr) : ref(*ptr) {}

        T& ref;

        //! Register the memory address of the class with CRPSOutputMapper or CRPSInputMapper
        template<class Archive> inline
        typename std::enable_if<std::is_base_of<detail::CRPSMapperCore, Archive>::value, void>::type
        CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar)
        {
            CEREAL_SAVE_FUNCTION_NAME(ar, cereal::memory_detail::PtrWrapper<ThisPointer<T>&>(*this));
        }

        //! Representation for the user archive.
        template<class Archive> inline
        typename std::enable_if<!std::is_base_of<detail::CRPSMapperCore, Archive>::value, void>::type
        CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar)
        {

        }
    };

    // ######################################################################
    //! Creates a ThisPointer that stores an l-value reference to a class T. 
    /*! Creates a ThisPointer that stores an l-value reference to a class T. 

        Example: 
        @code{cpp}
        class Point 
        {
            float x, y;

            template<class Archive>
            void serialize(Archive& ar) 
            {
                ar(x, y, this_ptr(this));
            }
        };
        @endcode

        @relates ThisPointer
        @ingroup Utility 
        */
    template<class T> inline
    ThisPointer<T> this_ptr(T* ptr)
    {
        return { ptr };
    }

    // ######################################################################
    //! A class for raw pointers that can be used in STL containers and classes.
    /*! A class for raw pointers that can be used in STL containers and classes. 
        Automatically generates a RawPointer<T> when archived, without requring 
        the raw pointer to be wrapped in a make_raw_ptr call in a class archive 
        method. 
        @internal */
    template<class T>
    class raw_ptr
    {
    public:

        raw_ptr(T* ptr) : ptr(ptr) {}
        raw_ptr() : raw_ptr(nullptr) {}

        T* ptr;

        //! Register the pointer with CRPSOutputMapper or CRPSInputMapper
        template<class Archive> inline
        typename std::enable_if<std::is_base_of<detail::CRPSMapperCore, Archive>::value, void>::type
        CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar)
        {
            ar(make_raw_ptr(ptr));
        }

        //! Representation for the user archive.
        template<class Archive> inline
        typename std::enable_if<!std::is_base_of<detail::CRPSMapperCore, Archive>::value, void>::type
        CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar)
        {

        }

        //! retrieve pointer
        T* get()
        {
            return ptr;
        }

        //! dereference pointer
        T& operator*()
        {
            return *ptr;
        }
    };


    // forward decls
    class CRPSInputMapper;
    class CRPSOutputMapper;

    // ######################################################################
    //! A wrapper that enables serializing raw pointers for output archives.    
    /*! This class enables an archive to be used to serialize raw pointers 
        that point to types co-serialized by the graph traversal. It provides 
        an archive interface that wraps the interface of the provided user archive, 
        to intercept objects and store their memory addresses.  

        Upon ~CRPSOutputArchive or CRPSOutputArchive::complete, 
        a vector used for mapping pointer indexes to object traversal indexes 
        is passed into the user provided archive to be serialized. An exception 
        is thrown if serialization is attempted after CRPSOutputArchive::complete. 
         
        Tracking addresses of BinaryData is not currently supported. 

        @endcode */
    template<class Archive>
    class CRPSOutputArchive
    {
    public:

        /*! @param archive The archive provided by the user, its interface is wrapped for object tracking. */
        CRPSOutputArchive(Archive& archive) : archive(archive)
        {
            static_assert(Archive::is_saving::value, "CRPSOutputArchive<Archive> cannot be used with an input archive.");
        }

        /*! Complete defered action if not completed. */
        ~CRPSOutputArchive()
        {
            complete();
        }

        /*! For OutputArchive: generates pointer book-keeping and saves book-keeping to output_archive, 
            For InputArchive: loads book-keeping from input_archive and does defered pointer initialization. 

            @throws CRPSException If pointer-booking serialization or defered pointer initialization fails. 
        */
        void complete()
        {
            if (completed) {
                return;
            }
            completed = true;

            archive.serializeDeferments();
            pointer_mapper.complete(archive);
        }

        //! Forwards types to user archive and crps mapper. 
        template <class ... Types> inline
        CRPSOutputArchive& operator()(Types&& ... args)
        {
            handle(std::forward<Types>(args)...);
            return *this;
        }

        //! This is a boost compatability layer.
        template <class T> inline
        CRPSOutputArchive& operator&(T&& arg)
        {
            handle(std::forward<T>(arg));
            return *this;
        }

        //! This is a boost compatability layer.
        template <class T> inline
        CRPSOutputArchive& operator<<(T&& arg)
        {
            handle(std::forward<T>(arg));
            return *this;
        }

    private:

        /*! Forwards user archive and crps mapper with types. 
            @throws CRPSException If attempted serialization afterCRPSArchiveBase::complete called. 
        */
        template <class ... Types> inline
        void handle(Types&& ... args)
        {
            if (completed) {
                throw CRPSException("Attempted serialization after CRPSArchiveBase::complete called");
            }

            archive(std::forward<Types>(args)...);
            pointer_mapper(std::forward<Types>(args)...);
        }

    private:
        Archive& archive; //!< User provided serialization archive

        CRPSOutputMapper pointer_mapper; //!< type is CRPSOutputMapper if Archive::is_saving, otherwise CRPSInputMapper

        bool completed{ false }; //!< True if CRPSOutputMapper or CRPSInputMapper complete method has been called
    };
    
    // ###################################################################### 
    //! A wrapper that enables serializing raw pointers for output archives.
    /*! This class enables an archive to be used to serialize raw pointers 
        that point to types co-serialized by the graph traversal. It provides 
        an archive interface that wraps the interface of the provided user archive, 
        to intercept objects and store their memory addresses. 

        Upon ~CRPSInputArchive or CRPSInputArchive::complete, 
        a vector used for mapping pointer indexes to object traversal indexes 
        is loaded from the user provided archive and is used to restore the 
        association of pointers relative to object traversal indexes. An exception 
        is thrown if serialization is attempted after CRPSInputArchive::complete. 

        Tracking addresses of BinaryData is not currently supported.

        @endcode */

    template<class Archive>
    class CRPSInputArchive
    {
    public:

        /*! @param archive The archive provided by the user, its interface is wrapped for object tracking. */
        CRPSInputArchive(Archive& archive) : archive(archive)
        { 
            static_assert(Archive::is_loading::value, "CRPSInputArchive<Archive> cannot be used with an output archive.");
        }

        /*! Complete defered action if not completed. */
        ~CRPSInputArchive()
        {
            complete();
        }

        /*! For OutputArchive: generates pointer book-keeping and saves book-keeping to output_archive,
            For InputArchive: loads book-keeping from input_archive and does defered pointer initialization.

            @throws CRPSException If pointer-booking serialization or defered pointer initialization fails.
        */
        void complete()
        {
            if (completed) {
                return;
            }
            completed = true;

            archive.serializeDeferments();
            pointer_mapper.complete(archive);
        }

        //! Forwards types to user archive and crps mapper. 
        template <class ... Types> inline
        CRPSInputArchive& operator()(Types&& ... args)
        {
            handle(std::forward<Types>(args)...);
            return *this;
        }

        //! This is a boost compatability layer.
        template <class T> inline
        CRPSInputArchive& operator&(T&& arg)
        {
            handle(std::forward<T>(arg));
            return *this;
        }

        //! This is a boost compatability layer.
        template <class T> inline
        CRPSInputArchive& operator>>(T&& arg)
        {
            handle(std::forward<T>(arg));
            return *this;
        }

    private:

        /*! Forwards user archive and crps mapper with types.
            @throws CRPSException If attempted serialization afterCRPSArchiveBase::complete called.
        */
        template <class ... Types> inline
        void handle(Types&& ... args)
        {
            if (completed) {
                throw CRPSException("Attempted serialization after CRPSArchiveBase::complete called");
            }
            archive(std::forward<Types>(args)...);
            pointer_mapper(std::forward<Types>(args)...);
        }

    private:
        Archive& archive; //!< User provided serialization archive

        CRPSInputMapper pointer_mapper; //!< type is CRPSOutputMapper if Archive::is_saving, otherwise CRPSInputMapper

        bool completed{ false }; //!< True if CRPSOutputMapper or CRPSInputMapper complete method has been called
    };

    // ###################################################################### 
    //! Performs pointer book-keeping when saving classes to an OutputArchive. 
    /*! This class is used by CRPSOutputArchive to associate the memory address
        of each object or pointer it encounters with a graph traversal id. 
        CRPSInputArchive then provides this class with the user's archive to 
        save book-keeping (in the form of a pointer-id to object-id map) created 
        by the graph traversal data. 

        @internal */
    class CRPSOutputMapper : public cereal::OutputArchive<CRPSOutputMapper, cereal::AllowEmptyClassElision>, public detail::CRPSMapperCore
    {
    public:

        //! Empty object_address_to_object_id map associates an address of nullptr to an object-id of 0
        CRPSOutputMapper() :
            OutputArchive<CRPSOutputMapper, cereal::AllowEmptyClassElision>(this)
        {
            obj_ptr_to_id[nullptr] = map_insert_count++;
        }

        /*! Creates pointer_id_to_object_id map from pointers/objects tracked from traversal. 
            Saves map to output_archive. 
   
            @param output_archive A copy of the users's output_archive reference stored in the CRPSInputArchive 
            */
        template <class Archive>
        void complete(Archive& output_archive)
        {
            std::vector<std::uint32_t> raw_to_obj{};

            for (const auto rpv : raw_ptr_values) 
            {
                if (obj_ptr_to_id.count(rpv) == 0) 
                {
                    std::ostringstream address{};
                    address << rpv;
                    throw CRPSException("Memory address " + address.str() + " not found in serialization traversal");
                }
                raw_to_obj.push_back(obj_ptr_to_id[rpv]);
            }

            output_archive(raw_to_obj);
        }

        //! Associate the object memory address with next object id
        template <class T> inline
        void trackAddress(T const& t)
        {
            obj_ptr_to_id[std::addressof(t)] = map_insert_count++;
        }

        //! Associate the pointer value with next pointer id, and track it as an object
        template <class T> inline
        void trackPointer(T* const& p)
        {
            raw_ptr_values.push_back(p);
            trackAddress(p);
        }

    private:
        std::unordered_map<const void*, std::uint32_t> obj_ptr_to_id{}; //!< Associates object memory address with object-id

        std::uint32_t map_insert_count{}; //!< Next available object-id

        std::vector<const void*> raw_ptr_values{}; //!< Associates pointer value to pointer-id

        bool completed{ false }; //!< True if pointer book-keeping is saved to user archive
    };

    // ######################################################################  
    //! Performs pointer book-keeping when loading classes from an InputArchive. 
    /*! This class is used by CRPSInputArchive to associate the memory address
        of each object or pointer it encounters with a graph traversal id. 
        CRPSInputArchive then provides this class with the user's archive to 
        load book-keeping (in the form of a pointer-id to object-id map) to 
        initialize the pointers. 

        @internal */
    class CRPSInputMapper : public cereal::OutputArchive<CRPSInputMapper, cereal::AllowEmptyClassElision>, public detail::CRPSMapperCore
    {
    public:

        //! Empty object_id_to_object_address map associates an object-id of 0 to a memory address of nullptr 
        CRPSInputMapper() :
            OutputArchive<CRPSInputMapper, cereal::AllowEmptyClassElision>(this)
        {
            obj_ptrs.push_back(nullptr);
        }

        /*! Loads pointer_id_to_object_id map from input_archive.
            Performs defered pointer initializations (with pointers/objects tracked in traversal) using pointer_id_to_object_id map. 

            @param input_archive A copy of the users's input_archive reference stored in the CRPSInputArchive 
            */
        template <class Archive>
        void complete(Archive& input_archive)
        {
            std::vector<std::uint32_t> raw_to_obj{};
            input_archive(raw_to_obj);
            
            if (raw_to_obj.size() != raw_ptrs.size()) {
                throw CRPSException("Size of raw_ptr_to_obj_id map loaded from input archive does not match size of map generated from traversal");
            }

            for (int i = 0; i < raw_ptrs.size(); i++)
            {
                if (raw_to_obj[i] >= obj_ptrs.size())
                {
                    std::ostringstream address{};
                    address << raw_ptrs[i];
                    throw CRPSException("Pointer at memory address " + address.str() + " has object index exceeding object traversal count");
                }
                deferedPtrLoads[i](raw_ptrs[i], obj_ptrs[raw_to_obj[i]]);
            }
        }

        //! Associate the object id to the object memory address.
        template <class T> inline
        void trackAddress(T& t)
        {
            obj_ptrs.push_back(std::addressof(t));
        }

        //! Associate the pointer id to the pointer's memory address, and track it as an object
        template <class T> inline
        void trackPointer(T*& p)
        {
            raw_ptrs.push_back(std::addressof(p));
            std::function<void(void*, void*)> deferment([](void* raw, void* obj_address) { *static_cast<T**>(raw) = static_cast<T*>(obj_address); });
            deferedPtrLoads.emplace_back(std::move(deferment));

            trackAddress(p);
        }

    private:
        std::vector<void*> obj_ptrs{}; //!< Associates object-id with an object's memory address

        std::vector<void*> raw_ptrs{}; //!< Associates pointer-id with a pointer's memory address
        std::vector<std::function<void(void*, void*)>> deferedPtrLoads{}; //!< Type-specific pointer initialization functions

        bool completed{ false }; //!< True if defered pointer initialization is completed
    };

    //! Track memory address of POD types for defered saving of pointer associations
    template <class T> inline
    typename std::enable_if<std::is_arithmetic<T>::value, void>::type
    CEREAL_SAVE_FUNCTION_NAME(CRPSOutputMapper& ar, T const& t)
    {
        ar.trackAddress(t);
    }

    //! Track memory address of POD types for defered loading of pointer associations
    template <class T> inline
    typename std::enable_if<std::is_arithmetic<T>::value, void>::type
    CEREAL_SAVE_FUNCTION_NAME(CRPSInputMapper& ar, T const& t)
    {
        ar.trackAddress(const_cast<T&>(t));
    }

    //! Track memory address of class types for defered saving of pointer associations
    template<class T> inline
    void CEREAL_SAVE_FUNCTION_NAME(CRPSOutputMapper& ar, PtrWrapper<ThisPointer<T>&> const& t)
    {
        ar.trackAddress(t.ptr.ref);
    }

    //! Track memory address of class types for defered loading of pointer associations
    template<class T> inline
    void CEREAL_SAVE_FUNCTION_NAME(CRPSInputMapper& ar, PtrWrapper<ThisPointer<T>&> const& t)
    {
        ar.trackAddress(t.ptr.ref);
    }

    /*! Track address of types in NameValuePair wrapper.
        t.value may be a wrapper type that must be unpacked, so  &(t.value) is not directly taken 
        */
    template <class Archive, class T> inline
    CEREAL_ARCHIVE_RESTRICT(CRPSInputMapper, CRPSOutputMapper)
    CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar, cereal::NameValuePair<T>& t)
    {
        ar(t.value);
    }

    /*! Track address of sizes in SizeTag wrapper.
        It is possible for t.size not to be an l-value reference,
        but it's easier to track but ignore a dangling pointer than to test for this. 
        */
    template <class Archive, class T> inline
    CEREAL_ARCHIVE_RESTRICT(CRPSInputMapper, CRPSOutputMapper)
    CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar, cereal::SizeTag<T>& t)
    {
        ar(t.size);
    }

    //! Track initialized pointer's value for defered saving of pointer associations
    template <class T> inline
    void CEREAL_SAVE_FUNCTION_NAME(CRPSOutputMapper& ar, PtrWrapper<T*&> const& rpw)
    {
        ar.trackPointer(rpw.ptr);
    }

    //! track uninitialized pointer's memory address for defered pointer initialization
    template <class T> inline
    void CEREAL_SAVE_FUNCTION_NAME(CRPSInputMapper& ar, PtrWrapper<T*&> const& rpw)
    {
        ar.trackPointer(rpw.ptr);
    }

    //! Do-nothing specialization for binary data
    template <class T> inline
    void CEREAL_SAVE_FUNCTION_NAME(CRPSOutputMapper& ar, cereal::BinaryData<T> const& bd)
    {
    
    }

    //! Do-nothing specialization for binary data
    template <class T> inline
    void CEREAL_SAVE_FUNCTION_NAME(CRPSInputMapper& ar, cereal::BinaryData<T> const& bd)
    {
    
    }

}


CEREAL_REGISTER_ARCHIVE(crps::CRPSOutputMapper)
CEREAL_REGISTER_ARCHIVE(crps::CRPSInputMapper)

CEREAL_SETUP_ARCHIVE_TRAITS(crps::CRPSInputMapper, crps::CRPSOutputMapper)

#endif