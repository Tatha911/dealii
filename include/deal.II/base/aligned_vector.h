// ---------------------------------------------------------------------
//
// Copyright (C) 2011 - 2020 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------


#ifndef dealii_aligned_vector_h
#define dealii_aligned_vector_h

#include <deal.II/base/config.h>

#include <deal.II/base/exceptions.h>
#include <deal.II/base/memory_consumption.h>
#include <deal.II/base/parallel.h>
#include <deal.II/base/utilities.h>

// boost::serialization::make_array used to be in array.hpp, but was
// moved to a different file in BOOST 1.64
#include <boost/version.hpp>
#if BOOST_VERSION >= 106400
#  include <boost/serialization/array_wrapper.hpp>
#else
#  include <boost/serialization/array.hpp>
#endif
#include <boost/serialization/split_member.hpp>

#include <cstring>
#include <memory>
#include <type_traits>



DEAL_II_NAMESPACE_OPEN


/**
 * This is a replacement class for std::vector to be used in combination with
 * VectorizedArray and derived data types. It allocates memory aligned to
 * addresses of a vectorized data type (in order to avoid segmentation faults
 * when a variable of type VectorizedArray which the compiler assumes to be
 * aligned to certain memory addresses does not actually follow these rules).
 * This could also be achieved by proving std::vector with a user-defined
 * allocator. On the other hand, writing an own small vector class lets us
 * implement parallel copy and move operations with TBB, insert deal.II-style
 * assertions, and cut some unnecessary functionality. Note that this vector
 * is a bit more memory-consuming than std::vector because of alignment, so it
 * is recommended to only use this vector on long vectors.
 *
 * @p author Katharina Kormann, Martin Kronbichler, 2011
 */
template <class T>
class AlignedVector
{
public:
  /**
   * Declare standard types used in all containers. These types parallel those
   * in the <tt>C++</tt> standard libraries <tt>vector<...></tt> class.
   */
  using value_type      = T;
  using pointer         = value_type *;
  using const_pointer   = const value_type *;
  using iterator        = value_type *;
  using const_iterator  = const value_type *;
  using reference       = value_type &;
  using const_reference = const value_type &;
  using size_type       = std::size_t;

  /**
   * Empty constructor. Sets the vector size to zero.
   */
  AlignedVector();

  /**
   * Set the vector size to the given size and initializes all elements with
   * T().
   *
   * @dealiiOperationIsMultithreaded
   */
  explicit AlignedVector(const size_type size, const T &init = T());

  /**
   * Destructor.
   */
  ~AlignedVector();

  /**
   * Copy constructor.
   *
   * @dealiiOperationIsMultithreaded
   */
  AlignedVector(const AlignedVector<T> &vec);

  /**
   * Move constructor. Create a new aligned vector by stealing the contents of
   * @p vec.
   */
  AlignedVector(AlignedVector<T> &&vec) noexcept;

  /**
   * Assignment to the input vector @p vec.
   *
   * @dealiiOperationIsMultithreaded
   */
  AlignedVector &
  operator=(const AlignedVector<T> &vec);

  /**
   * Move assignment operator.
   */
  AlignedVector &
  operator=(AlignedVector<T> &&vec) noexcept;

  /**
   * Change the size of the vector. If the new size is larger than the
   * previous size, then new elements will be added to the end of the
   * vector; these elements will remain uninitialized (i.e., left in
   * an undefined state) if `std::is_trivial<T>` is `true`, and
   * will be default initialized if `std::is_trivial<T>` is `false`.
   * See [here](https://en.cppreference.com/w/cpp/types/is_trivial) for
   * a definition of what `std::is_trivial` does.
   *
   * If the new size is less than the previous size, then the last few
   * elements will be destroyed if `std::is_trivial<T>` will be `false`
   * or will simply be ignored in the future if
   * `std::is_trivial<T>` is `true`.
   *
   * As a consequence of the outline above, the "_fast" suffix of this
   * function refers to the fact that for "trivial" classes `T`, this
   * function omits constructor/destructor calls and in particular the
   * initialization of new elements.
   *
   * @note This method can only be invoked for classes @p T that define a
   * default constructor, @p T(). Otherwise, compilation will fail.
   */
  void
  resize_fast(const size_type new_size);

  /**
   * Change the size of the vector. It keeps old elements previously
   * available, and initializes each newly added element to a
   * default-constructed object of type @p T.
   *
   * If the new vector size is shorter than the old one, the memory is
   * not immediately released unless the new size is zero; however,
   * the size of the current object is of course set to the requested
   * value. The destructors of released elements are also called.
   *
   * @dealiiOperationIsMultithreaded
   */
  void
  resize(const size_type new_size);

  /**
   * Change the size of the vector. It keeps old elements previously
   * available, and initializes each newly added element with the
   * provided initializer.
   *
   * If the new vector size is shorter than the old one, the memory is
   * not immediately released unless the new size is zero; however,
   * the size of the current object is of course set to the requested
   * value.
   *
   * @note This method can only be invoked for classes that define the copy
   * assignment operator. Otherwise, compilation will fail.
   *
   * @dealiiOperationIsMultithreaded
   */
  void
  resize(const size_type new_size, const T &init);

  /**
   * Reserve memory space for @p new_allocated_size elements.
   *
   * If the argument @p new_allocated_size is less than the current number of stored
   * elements (as indicated by calling size()), then this function does not
   * do anything at all. Except if the argument @p new_allocated_size is set
   * to zero, then all previously allocated memory is released (this operation
   * then being equivalent to directly calling the clear() function).
   *
   * In order to avoid too frequent reallocation (which involves copy of the
   * data), this function doubles the amount of memory occupied when the given
   * size is larger than the previously allocated size.
   *
   * Note that this function only changes the amount of elements the object
   * *can* store, but not the number of elements it *actually* stores. As
   * a consequence, no constructors or destructors of newly created objects
   * are run, though the existing elements may be moved to a new location (which
   * involves running the move constructor at the new location and the
   * destructor at the old location).
   */
  void
  reserve(const size_type new_allocated_size);

  /**
   * Releases all previously allocated memory and leaves the vector in a state
   * equivalent to the state after the default constructor has been called.
   */
  void
  clear();

  /**
   * Inserts an element at the end of the vector, increasing the vector size
   * by one. Note that the allocated size will double whenever the previous
   * space is not enough to hold the new element.
   */
  void
  push_back(const T in_data);

  /**
   * Return the last element of the vector (read and write access).
   */
  reference
  back();

  /**
   * Return the last element of the vector (read-only access).
   */
  const_reference
  back() const;

  /**
   * Inserts several elements at the end of the vector given by a range of
   * elements.
   */
  template <typename ForwardIterator>
  void
  insert_back(ForwardIterator begin, ForwardIterator end);

  /**
   * Fills the vector with size() copies of a default constructed object.
   *
   * @note Unlike the other fill() function, this method can also be
   * invoked for classes that do not define a copy assignment
   * operator.
   *
   * @dealiiOperationIsMultithreaded
   */
  void
  fill();

  /**
   * Fills the vector with size() copies of the given input.
   *
   * @note This method can only be invoked for classes that define the copy
   * assignment operator. Otherwise, compilation will fail.
   *
   * @dealiiOperationIsMultithreaded
   */
  void
  fill(const T &element);

  /**
   * Swaps the given vector with the calling vector.
   */
  void
  swap(AlignedVector<T> &vec);

  /**
   * Return whether the vector is empty, i.e., its size is zero.
   */
  bool
  empty() const;

  /**
   * Return the size of the vector.
   */
  size_type
  size() const;

  /**
   * Return the capacity of the vector, i.e., the size this vector can hold
   * without reallocation. Note that capacity() >= size().
   */
  size_type
  capacity() const;

  /**
   * Read-write access to entry @p index in the vector.
   */
  reference operator[](const size_type index);

  /**
   * Read-only access to entry @p index in the vector.
   */
  const_reference operator[](const size_type index) const;

  /**
   * Return a pointer to the underlying data buffer.
   */
  pointer
  data();

  /**
   * Return a const pointer to the underlying data buffer.
   */
  const_pointer
  data() const;

  /**
   * Return a read and write pointer to the beginning of the data array.
   */
  iterator
  begin();

  /**
   * Return a read and write pointer to the end of the data array.
   */
  iterator
  end();

  /**
   * Return a read-only pointer to the beginning of the data array.
   */
  const_iterator
  begin() const;

  /**
   * Return a read-only pointer to the end of the data array.
   */
  const_iterator
  end() const;

  /**
   * Return the memory consumption of the allocated memory in this class. If
   * the underlying type @p T allocates memory by itself, this memory is not
   * counted.
   */
  size_type
  memory_consumption() const;

  /**
   * Write the data of this object to a stream for the purpose of
   * serialization using the [BOOST serialization
   * library](https://www.boost.org/doc/libs/1_74_0/libs/serialization/doc/index.html).
   */
  template <class Archive>
  void
  save(Archive &ar, const unsigned int version) const;

  /**
   * Read the data of this object from a stream for the purpose of
   * serialization using the [BOOST serialization
   * library](https://www.boost.org/doc/libs/1_74_0/libs/serialization/doc/index.html).
   */
  template <class Archive>
  void
  load(Archive &ar, const unsigned int version);

#ifdef DOXYGEN
  /**
   * Write and read the data of this object from a stream for the purpose
   * of serialization using the [BOOST serialization
   * library](https://www.boost.org/doc/libs/1_74_0/libs/serialization/doc/index.html).
   */
  template <class Archive>
  void
  serialize(Archive &archive, const unsigned int version);
#else
  // This macro defines the serialize() method that is compatible with
  // the templated save() and load() method that have been implemented.
  BOOST_SERIALIZATION_SPLIT_MEMBER()
#endif

private:
  /**
   * Pointer to actual data array.
   */
  std::unique_ptr<T[], void (*)(T *)> elements;

  /**
   * Pointer to one past the last valid value.
   */
  T *used_elements_end;

  /**
   * Pointer to the end of the allocated memory.
   */
  T *allocated_elements_end;
};


// ------------------------------- inline functions --------------------------

/**
 * This namespace defines the copy and set functions used in AlignedVector.
 * These functions operate in parallel when there are enough elements in the
 * vector.
 */
namespace internal
{
  /**
   * A class that actually issues the copy commands in AlignedVector.
   * This class is based on the specialized for loop base class
   * ParallelForLoop in parallel.h whose purpose is the following: When
   * calling a parallel for loop on AlignedVector with apply_to_subranges, it
   * generates different code for every different argument we might choose (as
   * it is templated). This gives a lot of code (e.g. it triples the memory
   * required for compiling the file matrix_free.cc and the final object size
   * is several times larger) which is completely useless. Therefore, this
   * class channels all copy commands through one call to apply_to_subrange
   * for all possible types, which makes the copy operation much cleaner
   * (thanks to a virtual function, whose cost is negligible in this context).
   *
   * @relatesalso AlignedVector
   */
  template <typename T>
  class AlignedVectorCopy : private dealii::parallel::ParallelForInteger
  {
    static const std::size_t minimum_parallel_grain_size =
      160000 / sizeof(T) + 1;

  public:
    /**
     * Constructor. Issues a parallel call if there are sufficiently many
     * elements, otherwise works in serial. Copies the data from the half-open
     * interval between @p source_begin and @p source_end to array starting at
     * @p destination (by calling the copy constructor with placement new).
     *
     * The elements from the source array are simply copied via the placement
     * new copy constructor.
     */
    AlignedVectorCopy(const T *const source_begin,
                      const T *const source_end,
                      T *const       destination)
      : source_(source_begin)
      , destination_(destination)
    {
      Assert(source_end >= source_begin, ExcInternalError());
      Assert(source_end == source_begin || destination != nullptr,
             ExcInternalError());
      const std::size_t size = source_end - source_begin;
      if (size < minimum_parallel_grain_size)
        AlignedVectorCopy::apply_to_subrange(0, size);
      else
        apply_parallel(0, size, minimum_parallel_grain_size);
    }

    /**
     * This method moves elements from the source to the destination given in
     * the constructor on a subrange given by two integers.
     */
    virtual void
    apply_to_subrange(const std::size_t begin,
                      const std::size_t end) const override
    {
      if (end == begin)
        return;

      // for classes trivial assignment can use memcpy. cast element to
      // (void*) to silence compiler warning for virtual classes (they will
      // never arrive here because they are non-trivial).

      if (std::is_trivial<T>::value == true)
        std::memcpy(static_cast<void *>(destination_ + begin),
                    static_cast<const void *>(source_ + begin),
                    (end - begin) * sizeof(T));
      else
        for (std::size_t i = begin; i < end; ++i)
          new (&destination_[i]) T(source_[i]);
    }

  private:
    const T *const source_;
    T *const       destination_;
  };


  /**
   * Like AlignedVectorCopy, but use move operations instead of copy operations.
   *
   * @relatesalso AlignedVector
   */
  template <typename T>
  class AlignedVectorMove : private dealii::parallel::ParallelForInteger
  {
    static const std::size_t minimum_parallel_grain_size =
      160000 / sizeof(T) + 1;

  public:
    /**
     * Constructor. Issues a parallel call if there are sufficiently many
     * elements, otherwise works in serial. Moves the data from the half-open
     * interval between @p source_begin and @p source_end to array starting at
     * @p destination (by calling the move constructor with placement new).
     *
     * The data is moved between the two arrays by invoking the destructor on
     * the source range (preparing for a subsequent call to free).
     */
    AlignedVectorMove(T *const source_begin,
                      T *const source_end,
                      T *const destination)
      : source_(source_begin)
      , destination_(destination)
    {
      Assert(source_end >= source_begin, ExcInternalError());
      Assert(source_end == source_begin || destination != nullptr,
             ExcInternalError());
      const std::size_t size = source_end - source_begin;
      if (size < minimum_parallel_grain_size)
        AlignedVectorMove::apply_to_subrange(0, size);
      else
        apply_parallel(0, size, minimum_parallel_grain_size);
    }

    /**
     * This method moves elements from the source to the destination given in
     * the constructor on a subrange given by two integers.
     */
    virtual void
    apply_to_subrange(const std::size_t begin,
                      const std::size_t end) const override
    {
      if (end == begin)
        return;

      // for classes trivial assignment can use memcpy. cast element to
      // (void*) to silence compiler warning for virtual classes (they will
      // never arrive here because they are non-trivial).

      if (std::is_trivial<T>::value == true)
        std::memcpy(static_cast<void *>(destination_ + begin),
                    static_cast<void *>(source_ + begin),
                    (end - begin) * sizeof(T));
      else
        for (std::size_t i = begin; i < end; ++i)
          {
            // initialize memory (copy construct by placement new), and
            // destruct the source
            new (&destination_[i]) T(std::move(source_[i]));
            source_[i].~T();
          }
    }

  private:
    T *const source_;
    T *const destination_;
  };


  /**
   * Class that issues the set commands for AlignedVector.
   *
   * @tparam initialize_memory Sets whether the set command should
   * initialize memory (with a call to the copy constructor) or rather use the
   * copy assignment operator. A template is necessary to select the
   * appropriate operation since some classes might define only one of those
   * two operations.
   *
   * @relatesalso AlignedVector
   */
  template <typename T, bool initialize_memory>
  class AlignedVectorSet : private dealii::parallel::ParallelForInteger
  {
    static const std::size_t minimum_parallel_grain_size =
      160000 / sizeof(T) + 1;

  public:
    /**
     * Constructor. Issues a parallel call if there are sufficiently many
     * elements, otherwise work in serial.
     */
    AlignedVectorSet(const std::size_t size,
                     const T &         element,
                     T *const          destination)
      : element_(element)
      , destination_(destination)
      , trivial_element(false)
    {
      if (size == 0)
        return;
      Assert(destination != nullptr, ExcInternalError());

      // do not use memcmp for long double because on some systems it does not
      // completely fill its memory and may lead to false positives in
      // e.g. valgrind
      if (std::is_trivial<T>::value == true &&
          std::is_same<T, long double>::value == false)
        {
          const unsigned char zero[sizeof(T)] = {};
          // cast element to (void*) to silence compiler warning for virtual
          // classes (they will never arrive here because they are
          // non-trivial).
          if (std::memcmp(zero,
                          static_cast<const void *>(&element),
                          sizeof(T)) == 0)
            trivial_element = true;
        }
      if (size < minimum_parallel_grain_size)
        AlignedVectorSet::apply_to_subrange(0, size);
      else
        apply_parallel(0, size, minimum_parallel_grain_size);
    }

    /**
     * This sets elements on a subrange given by two integers.
     */
    virtual void
    apply_to_subrange(const std::size_t begin,
                      const std::size_t end) const override
    {
      // for classes with trivial assignment of zero can use memset. cast
      // element to (void*) to silence compiler warning for virtual
      // classes (they will never arrive here because they are
      // non-trivial).
      if (std::is_trivial<T>::value == true && trivial_element)
        std::memset(static_cast<void *>(destination_ + begin),
                    0,
                    (end - begin) * sizeof(T));
      else
        copy_construct_or_assign(
          begin, end, std::integral_constant<bool, initialize_memory>());
    }

  private:
    const T &  element_;
    mutable T *destination_;
    bool       trivial_element;

    // copy assignment operation
    void
    copy_construct_or_assign(const std::size_t begin,
                             const std::size_t end,
                             std::integral_constant<bool, false>) const
    {
      for (std::size_t i = begin; i < end; ++i)
        destination_[i] = element_;
    }

    // copy constructor (memory initialization)
    void
    copy_construct_or_assign(const std::size_t begin,
                             const std::size_t end,
                             std::integral_constant<bool, true>) const
    {
      for (std::size_t i = begin; i < end; ++i)
        new (&destination_[i]) T(element_);
    }
  };



  /**
   * Class that issues the set commands for AlignedVector.
   *
   * @tparam initialize_memory Sets whether the set command should
   * initialize memory (with a call to the copy constructor) or rather use the
   * copy assignment operator. A template is necessary to select the
   * appropriate operation since some classes might define only one of those
   * two operations.
   *
   * @relatesalso AlignedVector
   */
  template <typename T, bool initialize_memory>
  class AlignedVectorDefaultInitialize
    : private dealii::parallel::ParallelForInteger
  {
    static const std::size_t minimum_parallel_grain_size =
      160000 / sizeof(T) + 1;

  public:
    /**
     * Constructor. Issues a parallel call if there are sufficiently many
     * elements, otherwise work in serial.
     */
    AlignedVectorDefaultInitialize(const std::size_t size, T *const destination)
      : destination_(destination)
    {
      if (size == 0)
        return;
      Assert(destination != nullptr, ExcInternalError());

      if (size < minimum_parallel_grain_size)
        AlignedVectorDefaultInitialize::apply_to_subrange(0, size);
      else
        apply_parallel(0, size, minimum_parallel_grain_size);
    }

    /**
     * This initializes elements on a subrange given by two integers.
     */
    virtual void
    apply_to_subrange(const std::size_t begin,
                      const std::size_t end) const override
    {
      // for classes with trivial assignment of zero can use memset. cast
      // element to (void*) to silence compiler warning for virtual
      // classes (they will never arrive here because they are
      // non-trivial).
      if (std::is_trivial<T>::value == true)
        std::memset(static_cast<void *>(destination_ + begin),
                    0,
                    (end - begin) * sizeof(T));
      else
        default_construct_or_assign(
          begin, end, std::integral_constant<bool, initialize_memory>());
    }

  private:
    mutable T *destination_;

    // copy assignment operation
    void
    default_construct_or_assign(const std::size_t begin,
                                const std::size_t end,
                                std::integral_constant<bool, false>) const
    {
      for (std::size_t i = begin; i < end; ++i)
        destination_[i] = std::move(T());
    }

    // copy constructor (memory initialization)
    void
    default_construct_or_assign(const std::size_t begin,
                                const std::size_t end,
                                std::integral_constant<bool, true>) const
    {
      for (std::size_t i = begin; i < end; ++i)
        new (&destination_[i]) T;
    }
  };

} // end of namespace internal


#ifndef DOXYGEN


template <class T>
inline AlignedVector<T>::AlignedVector()
  : elements(nullptr, [](T *) { Assert(false, ExcInternalError()); })
  , used_elements_end(nullptr)
  , allocated_elements_end(nullptr)
{}



template <class T>
inline AlignedVector<T>::AlignedVector(const size_type size, const T &init)
  : elements(nullptr, [](T *) { Assert(false, ExcInternalError()); })
  , used_elements_end(nullptr)
  , allocated_elements_end(nullptr)
{
  if (size > 0)
    resize(size, init);
}



template <class T>
inline AlignedVector<T>::~AlignedVector()
{
  clear();
}



template <class T>
inline AlignedVector<T>::AlignedVector(const AlignedVector<T> &vec)
  : elements(nullptr, [](T *) { Assert(false, ExcInternalError()); })
  , used_elements_end(nullptr)
  , allocated_elements_end(nullptr)
{
  // copy the data from vec
  reserve(vec.size());
  used_elements_end = allocated_elements_end;
  internal::AlignedVectorCopy<T>(vec.elements.get(),
                                 vec.used_elements_end,
                                 elements.get());
}



template <class T>
inline AlignedVector<T>::AlignedVector(AlignedVector<T> &&vec) noexcept
  : elements(std::move(vec.elements))
  , used_elements_end(vec.used_elements_end)
  , allocated_elements_end(vec.allocated_elements_end)
{
  vec.elements               = nullptr;
  vec.used_elements_end      = nullptr;
  vec.allocated_elements_end = nullptr;
}



template <class T>
inline AlignedVector<T> &
AlignedVector<T>::operator=(const AlignedVector<T> &vec)
{
  resize(0);
  resize_fast(vec.used_elements_end - vec.elements.get());
  internal::AlignedVectorCopy<T>(vec.elements.get(),
                                 vec.used_elements_end,
                                 elements.get());
  return *this;
}



template <class T>
inline AlignedVector<T> &
AlignedVector<T>::operator=(AlignedVector<T> &&vec) noexcept
{
  clear();

  // Move the actual data
  elements = std::move(vec.elements);

  // Then also steal the other pointers and clear them in the original object:
  used_elements_end      = vec.used_elements_end;
  allocated_elements_end = vec.allocated_elements_end;

  vec.used_elements_end      = nullptr;
  vec.allocated_elements_end = nullptr;

  return *this;
}



template <class T>
inline void
AlignedVector<T>::resize_fast(const size_type new_size)
{
  const size_type old_size = size();
  if (std::is_trivial<T>::value == false && new_size < old_size)
    {
      // call destructor on fields that are released. doing it backward
      // releases the elements in reverse order as compared to how they were
      // created
      while (used_elements_end != elements.get() + new_size)
        (--used_elements_end)->~T();
    }
  reserve(new_size);
  used_elements_end = elements.get() + new_size;

  // need to still set the values in case the class is non-trivial because
  // virtual classes etc. need to run their (default) constructor
  if (std::is_trivial<T>::value == false && new_size > old_size)
    dealii::internal::AlignedVectorDefaultInitialize<T, true>(
      new_size - old_size, elements.get() + old_size);
}



template <class T>
inline void
AlignedVector<T>::resize(const size_type new_size)
{
  const size_type old_size = size();
  if (std::is_trivial<T>::value == false && new_size < old_size)
    {
      // call destructor on fields that are released. doing it backward
      // releases the elements in reverse order as compared to how they were
      // created
      while (used_elements_end != elements.get() + new_size)
        (--used_elements_end)->~T();
    }
  reserve(new_size);
  used_elements_end = elements.get() + new_size;

  // finally set the desired init values
  if (new_size > old_size)
    dealii::internal::AlignedVectorDefaultInitialize<T, true>(
      new_size - old_size, elements.get() + old_size);
}



template <class T>
inline void
AlignedVector<T>::resize(const size_type new_size, const T &init)
{
  const size_type old_size = size();
  if (std::is_trivial<T>::value == false && new_size < old_size)
    {
      // call destructor on fields that are released. doing it backward
      // releases the elements in reverse order as compared to how they were
      // created
      while (used_elements_end != elements.get() + new_size)
        (--used_elements_end)->~T();
    }
  reserve(new_size);
  used_elements_end = elements.get() + new_size;

  // finally set the desired init values
  if (new_size > old_size)
    dealii::internal::AlignedVectorSet<T, true>(new_size - old_size,
                                                init,
                                                elements.get() + old_size);
}



template <class T>
inline void
AlignedVector<T>::reserve(const size_type new_allocated_size)
{
  const size_type old_size           = used_elements_end - elements.get();
  const size_type old_allocated_size = allocated_elements_end - elements.get();
  if (new_allocated_size > old_allocated_size)
    {
      // if we continuously increase the size of the vector, we might be
      // reallocating a lot of times. therefore, try to increase the size more
      // aggressively
      const size_type new_size =
        std::max(new_allocated_size, 2 * old_allocated_size);

      // allocate and align along 64-byte boundaries (this is enough for all
      // levels of vectorization currently supported by deal.II)
      T *new_data_ptr;
      Utilities::System::posix_memalign(
        reinterpret_cast<void **>(&new_data_ptr), 64, new_size * sizeof(T));
      std::unique_ptr<T[], void (*)(T *)> new_data(new_data_ptr, [](T *ptr) {
        std::free(ptr);
      });

      // copy whatever elements we need to retain
      if (new_allocated_size > 0)
        dealii::internal::AlignedVectorMove<T>(elements.get(),
                                               elements.get() + old_size,
                                               new_data.get());

      // Now reset all of the member variables of the current object
      // based on the allocation above. Assigning to a std::unique_ptr
      // object also releases the previously pointed to memory.
      elements               = std::move(new_data);
      used_elements_end      = elements.get() + old_size;
      allocated_elements_end = elements.get() + new_size;
    }
  else if (new_allocated_size == 0)
    clear();
  else // size_alloc < allocated_size
    {} // nothing to do here
}



template <class T>
inline void
AlignedVector<T>::clear()
{
  if (elements != nullptr)
    {
      if (std::is_trivial<T>::value == false)
        while (used_elements_end != elements.get())
          (--used_elements_end)->~T();
    }
  elements               = nullptr;
  used_elements_end      = nullptr;
  allocated_elements_end = nullptr;
}



template <class T>
inline void
AlignedVector<T>::push_back(const T in_data)
{
  Assert(used_elements_end <= allocated_elements_end, ExcInternalError());
  if (used_elements_end == allocated_elements_end)
    reserve(std::max(2 * capacity(), static_cast<size_type>(16)));
  if (std::is_trivial<T>::value == false)
    new (used_elements_end++) T(in_data);
  else
    *used_elements_end++ = in_data;
}



template <class T>
inline typename AlignedVector<T>::reference
AlignedVector<T>::back()
{
  AssertIndexRange(0, size());
  T *field = used_elements_end - 1;
  return *field;
}



template <class T>
inline typename AlignedVector<T>::const_reference
AlignedVector<T>::back() const
{
  AssertIndexRange(0, size());
  const T *field = used_elements_end - 1;
  return *field;
}



template <class T>
template <typename ForwardIterator>
inline void
AlignedVector<T>::insert_back(ForwardIterator begin, ForwardIterator end)
{
  const unsigned int old_size = size();
  reserve(old_size + (end - begin));
  for (; begin != end; ++begin, ++used_elements_end)
    {
      if (std::is_trivial<T>::value == false)
        new (used_elements_end) T;
      *used_elements_end = *begin;
    }
}



template <class T>
inline void
AlignedVector<T>::fill()
{
  dealii::internal::AlignedVectorDefaultInitialize<T, false>(size(),
                                                             elements.get());
}



template <class T>
inline void
AlignedVector<T>::fill(const T &value)
{
  dealii::internal::AlignedVectorSet<T, false>(size(), value, elements.get());
}



template <class T>
inline void
AlignedVector<T>::swap(AlignedVector<T> &vec)
{
  std::swap(elements, vec.elements);
  std::swap(used_elements_end, vec.used_elements_end);
  std::swap(allocated_elements_end, vec.allocated_elements_end);
}



template <class T>
inline bool
AlignedVector<T>::empty() const
{
  return used_elements_end == elements.get();
}



template <class T>
inline typename AlignedVector<T>::size_type
AlignedVector<T>::size() const
{
  return used_elements_end - elements.get();
}



template <class T>
inline typename AlignedVector<T>::size_type
AlignedVector<T>::capacity() const
{
  return allocated_elements_end - elements.get();
}



template <class T>
inline typename AlignedVector<T>::reference AlignedVector<T>::
                                            operator[](const size_type index)
{
  AssertIndexRange(index, size());
  return elements[index];
}



template <class T>
inline typename AlignedVector<T>::const_reference AlignedVector<T>::
                                                  operator[](const size_type index) const
{
  AssertIndexRange(index, size());
  return elements[index];
}



template <typename T>
inline typename AlignedVector<T>::pointer
AlignedVector<T>::data()
{
  return elements.get();
}



template <typename T>
inline typename AlignedVector<T>::const_pointer
AlignedVector<T>::data() const
{
  return elements.get();
}



template <class T>
inline typename AlignedVector<T>::iterator
AlignedVector<T>::begin()
{
  return elements.get();
}



template <class T>
inline typename AlignedVector<T>::iterator
AlignedVector<T>::end()
{
  return used_elements_end;
}



template <class T>
inline typename AlignedVector<T>::const_iterator
AlignedVector<T>::begin() const
{
  return elements.get();
}



template <class T>
inline typename AlignedVector<T>::const_iterator
AlignedVector<T>::end() const
{
  return used_elements_end;
}



template <class T>
template <class Archive>
inline void
AlignedVector<T>::save(Archive &ar, const unsigned int) const
{
  size_type vec_size(size());
  ar &      vec_size;
  if (vec_size > 0)
    ar &boost::serialization::make_array(elements.get(), vec_size);
}



template <class T>
template <class Archive>
inline void
AlignedVector<T>::load(Archive &ar, const unsigned int)
{
  size_type vec_size = 0;
  ar &      vec_size;

  if (vec_size > 0)
    {
      reserve(vec_size);
      ar &boost::serialization::make_array(elements.get(), vec_size);
      used_elements_end = elements.get() + vec_size;
    }
}



template <class T>
inline typename AlignedVector<T>::size_type
AlignedVector<T>::memory_consumption() const
{
  size_type memory = sizeof(*this);
  for (const T *t = elements.get(); t != used_elements_end; ++t)
    memory += dealii::MemoryConsumption::memory_consumption(*t);
  memory += sizeof(T) * (allocated_elements_end - used_elements_end);
  return memory;
}


#endif // ifndef DOXYGEN


/**
 * Relational operator == for AlignedVector
 *
 * @relatesalso AlignedVector
 */
template <class T>
bool
operator==(const AlignedVector<T> &lhs, const AlignedVector<T> &rhs)
{
  if (lhs.size() != rhs.size())
    return false;
  for (typename AlignedVector<T>::const_iterator lit = lhs.begin(),
                                                 rit = rhs.begin();
       lit != lhs.end();
       ++lit, ++rit)
    if (*lit != *rit)
      return false;
  return true;
}



/**
 * Relational operator != for AlignedVector
 *
 * @relatesalso AlignedVector
 */
template <class T>
bool
operator!=(const AlignedVector<T> &lhs, const AlignedVector<T> &rhs)
{
  return !(operator==(lhs, rhs));
}


DEAL_II_NAMESPACE_CLOSE

#endif
