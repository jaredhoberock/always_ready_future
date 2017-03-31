#include <exception>
#include <type_traits>
#include "variant.hpp" // my compiler doesn't have <variant> yet
#include "optional.hpp" // my compiler doesn't have <optional> yet

// always_ready_future is intended to be a lightweight wrapper around
// a ready value that fulfills the basic requirements of the Future concept
// (whatever that may be someday)
//
// Executors which always block their client can use always_ready_future as their
// associated future and still expose two-way asynchronous execution functions like async_execute()

template<class T>
class always_ready_future
{
  public:
    always_ready_future(const T& value) : value_(value) {}

    always_ready_future(T&& value) : value_(std::move(value)) {}

    always_ready_future(std::exception_ptr e) : value_(e) {}

  private:
    struct get_visitor
    {
      T operator()(T& value) const
      {
        return std::move(value);
      }

      T operator()(std::exception_ptr& e) const
      {
        throw e;

        // XXX rework this visitor to avoid returning T in both cases
        return T();
      }
    };

  public:
    T get()
    {
      std::experimental::visit(get_visitor(), value_);
    }

    // arguably, waiting on an always_ready_future might indicate
    // a programmer error, but this type is intended to be used in
    // generic code where the concrete type of Future is not known in advance
    void wait() const
    {
      // wait() is a no-op: this is always ready
    }

    // there is no valid() to avoid having to store an extra bit
    // get() does not invalidate this future, but it does move its value
    // XXX should get() also set value_ to a future_error?
    
    // no share(), no then(), but there's no reason there couldn't be other than
    // implementation inconvenience

  private:
    // XXX we could avoid the overhead of this variant if
    //     we didn't allow this type to have an exceptional case
    //     the problem with that design is that this type is
    //     intended to be used in generic code where the concrete type of Future is not known
    //     and the way that generic clients interact with exceptional cases ought to be uniform
    std::experimental::variant<T,std::exception_ptr> value_;
};


template<>
class always_ready_future<void>
{
  public:
    always_ready_future() {}

    always_ready_future(std::exception_ptr e) : exception_(e) {}

  public:
    void get()
    {
      if(exception_)
      {
        throw exception_.value();
      }
    }

    // arguably, waiting on an always_ready_future might indicate
    // a programmer error, but this type is intended to be used in
    // generic code where the concrete type of Future is not known in advance
    void wait() const
    {
      // wait() is a no-op: this is always ready
    }

    // there is no valid() to avoid having to store an extra bit
    // get() does not invalidate this future, but it does move its value
    // XXX should get() also set exception_ to a future_error?
    
    // no share(), no then(), but there's no reason there couldn't be other than
    // implementation inconvenience

  private:
    // XXX we could avoid the overhead of this optional if
    //     we didn't allow this type to have an exceptional case
    //     the problem with that design is that this type is
    //     intended to be used in generic code where the concrete type of Future is not known
    //     and the way that generic clients interact with exceptional cases ought to be uniform
    std::experimental::optional<std::exception_ptr> exception_;
};


namespace detail
{


template<class Function,
         class Result = std::result_of_t<std::decay_t<Function>()>,
         class = std::enable_if_t<
           !std::is_void<Result>::value
         >>
always_ready_future<Result>
try_invoke(Function&& f)
{
  try
  {
    return always_ready_future<Result>(std::forward<Function>(f)());
  }
  catch(...)
  {
    return always_ready_future<Result>(std::current_exception());
  }
}


template<class Function,
         class Result = std::result_of_t<std::decay_t<Function>()>,
         class = std::enable_if_t<
           std::is_void<Result>::value
         >>
always_ready_future<void>
try_invoke(Function&& f)
{
  try
  {
    std::forward<Function>(f)();
    return always_ready_future<void>();
  }
  catch(...)
  {
    return always_ready_future<void>(std::current_exception());
  }
}


}

