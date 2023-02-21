#ifndef CPML_MATCH
#define CPML_MATCH

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "config.hpp"
#include "detail/as_tuple.hpp"
#include "detail/forwarder.hpp"
#include "lib.hpp"

namespace cpml::cpatterns {
  class match_error : public std::exception {
    public:
    virtual const char *what() const noexcept { return "match_error"; }
  };

  inline constexpr struct no_match_t {} no_match{};

  template <typename T>
  struct match_result : std::optional<detail::forwarder<T>> {
    using type = T;

    using super = std::optional<detail::forwarder<T>>;
    using super::super;

    match_result(no_match_t) noexcept {}
    match_result(std::nullopt_t) = delete;

    decltype(auto) get() && {
      return (*static_cast<super &&>(*this)).forward();
    }
  };

  template <typename T>
  inline constexpr bool is_match_result_v = false;

  template <typename T>
  inline constexpr bool is_match_result_v<match_result<T>> = true;

  template <typename F, typename... Args>
  auto match_invoke(F &&f, Args &&... args) {
    static_assert(lib::is_invocable_v<F, Args...>, "");
    using R = lib::invoke_result_t<F, Args...>;
    if constexpr (std::is_void_v<R>) {
      std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
      return match_result<void>(detail::void_{});
    } else if constexpr (is_match_result_v<R>) {
      return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    } else {
      return match_result<R>(
          std::invoke(std::forward<F>(f), std::forward<Args>(args)...));
    }
  }

  template <typename F, typename Args, std::size_t... Is>
  auto match_apply(F &&f, Args &&args, std::index_sequence<Is...>) {
    return match_invoke(std::forward<F>(f),
                        std::get<Is>(std::forward<Args>(args))...);
  }

  template <typename F, typename Args>
  auto match_apply(F &&f, Args &&args) {
    return match_apply(
        std::forward<F>(f),
        std::forward<Args>(args),
        std::make_index_sequence<std::tuple_size_v<std::decay_t<Args>>>{});
  }

  inline constexpr std::size_t npos = static_cast<std::size_t>(-1);


  template <typename ExprPattern, typename Value, typename F>
  auto try_match(const ExprPattern &expr_pattern, Value &&value, F &&f) {
    return expr_pattern == std::forward<Value>(value)
               ? match_invoke(std::forward<F>(f))
               : no_match;
  }

  namespace detail {

    template <typename F>
    struct LazyExpr;

    template <typename T>
    inline constexpr bool is_lazy_expr_v = false;

    template <typename F>
    inline constexpr bool is_lazy_expr_v<LazyExpr<F>> = true;

  } 

  template <std::size_t I, typename Pattern>
  struct Identifier;

  template <typename T>
  inline constexpr bool is_identifier_v = false;

  template <std::size_t I, typename Pattern>
  inline constexpr bool is_identifier_v<Identifier<I, Pattern>> = true;


  inline constexpr std::size_t wildcard_index = npos;
  inline constexpr std::size_t arg_index = npos / 2;

  namespace detail {

    template <std::size_t I, typename T>
    struct indexed_forwarder : forwarder<T> {
      static constexpr std::size_t index = I;

      using super = forwarder<T>;
      using super::super;
    };

    template <typename T>
    inline constexpr bool is_indexed_forwarder_v = false;

    template <std::size_t I, typename T>
    inline constexpr bool is_indexed_forwarder_v<indexed_forwarder<I, T>> = true;

    template <typename Is, typename... Ts>
    struct set;

    template <std::size_t... Is, typename... Ts>
    struct set<std::index_sequence<Is...>, Ts...>
        : lib::indexed_type<Is, Ts>... {};

    template <std::size_t I, typename T>
    struct find_indexed_forwarder;

    template <std::size_t I, typename... Ts>
    struct find_indexed_forwarder<I, std::tuple<Ts...>> {
      template <std::size_t Idx, typename T>
      static constexpr std::size_t impl(
          lib::indexed_type<Idx, indexed_forwarder<I, T> &&>) {
        return Idx;
      }

      static constexpr void impl(...) {
        static_assert(I == wildcard_index || I == arg_index);

        static_assert(I != wildcard_index,
                      "Reference to the wildcard pattern (`_`) in the `when` "
                      "clause is ambiguous. There are multiple instances of "
                      "them in the source pattern.");
        static_assert(I != arg_index,
                      "Reference to the arg pattern (`arg`) in the `when` "
                      "clause is ambiguous. There are multiple instances of "
                      "them in the source pattern.");
      }

      static constexpr std::size_t value =
          impl(set<std::index_sequence_for<Ts...>, Ts...>{});
    };

    template <std::size_t I, typename T>
    inline constexpr std::size_t find_indexed_forwarder_v =
        find_indexed_forwarder<I, T>::value;

    template <typename Arg, std::size_t... Is, typename... Ts>
    decltype(auto) eval(Arg &&arg,
                        std::tuple<indexed_forwarder<Is, Ts> &&...> &&ifs) {
      using Decayed = std::decay_t<Arg>;
      if constexpr (is_identifier_v<Decayed>) {
        if constexpr (Decayed::has_pattern) {
          return std::forward<Arg>(arg).as_lazy_expr().lambda(std::move(ifs));
        } else {
          constexpr std::size_t i = find_indexed_forwarder_v<
              Decayed::index,
              std::tuple<indexed_forwarder<Is, Ts> &&...>>;
          return std::get<i>(std::move(ifs)).forward();
        }
      } else if constexpr (is_lazy_expr_v<Decayed>) {
        return std::forward<Arg>(arg).lambda(std::move(ifs));
      } else {
        return std::forward<Arg>(arg);
      }
    }

  }  

  

#endif 
