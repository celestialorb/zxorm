#pragma once

#include "zxorm/common.hpp"
#include "zxorm/orm/types.hpp"
#include "zxorm/orm/constraints.hpp"
#include <type_traits>

namespace zxorm {

    namespace __constraint_t_detail {
        template <typename MemberT, typename... C>
        struct constraints_t {
            using type = unique_tuple<C...>;
        };

        template <typename MemberT, typename... C>
        requires (not ignore_qualifiers::is_optional<MemberT>())
        struct constraints_t<MemberT, C...> {
            using type = unique_tuple<NotNull<>, C...>;
        };

        static inline std::string constraint_creation_query(auto constraints) {
            std::stringstream ss;
            std::apply([&] (const auto&... c) {
                ([&](){
                    ss << c << " ";
                }(), ...);
            }, constraints);

            std::string qstr = ss.str();
            // erase trailing comma
            if (qstr.size() >= 2) {
                qstr.erase(qstr.end() - 1);
            }

            return qstr;
        }
    };

    template <FixedLengthString column_name, auto M, class... Constraint>
    class Column {
        template< typename T >
        struct find_column_traits : std::false_type
        {
            using type = std::false_type;
        };

        template< typename R, typename C, class A>
        struct find_column_traits< R (C::*)(A) >
        {
            using type = std::false_type;
        };

        template< typename R, typename C>
        struct find_column_traits< R C::* >
        {
            using type = R;
            using klass = C;
        };

        public:
        static constexpr bool public_column = true;
        using member_t = typename find_column_traits<decltype(M)>::type;
        static_assert(!std::is_same<member_t, std::false_type>::value, "Column template argument should be a pointer to a class member");
        using object_class = typename find_column_traits<decltype(M)>::klass;
        using constraints_t = typename __constraint_t_detail::constraints_t<member_t, Constraint...>::type;

        static constexpr sqlite_column_type sql_column_type = member_to_sql_type<member_t>::value;

        static constexpr bool is_primary_key = any_of<constraint_is_primary_key<Constraint>::value...>;
        static constexpr bool is_auto_inc_column = any_of<constraint_is_primary_key<Constraint>::value...> && sql_column_type == sqlite_column_type::INTEGER;

        static constexpr auto name = column_name;

        static auto& getter(auto& obj) { return obj.*M; };
        static void setter(auto& obj, auto arg) { obj.*M = arg; };

        static std::string constraint_creation_query() {
            return __constraint_t_detail::constraint_creation_query(constraints_t{});
        }
    };

    template <FixedLengthString column_name, auto Getter, auto Setter, class... Constraint>
    class ColumnPrivate {
        private:
        template< typename T >
        struct fn_ptr_traits : std::false_type
        {
            using arg_type = std::false_type;
            using return_type = void;
            using klass = std::false_type;
        };

        // pointer to setter
        template<typename C, class A >
        struct fn_ptr_traits< void (C::*)(A) >
        {
            using arg_type = A;
            using klass = C;
            using return_type = void;
        };

        // pointer to getter
        template< typename R, typename C  >
        struct fn_ptr_traits< R (C::*)() >
        {
            using arg_type = std::false_type;
            using klass = C;
            using return_type = R;
        };

        // pointer to member
        template< typename R, typename C>
        struct fn_ptr_traits< R C::* >
        {
            using arg_type = std::false_type;
            using return_type = void;
            using klass = std::false_type;
        };

        using setter_traits = fn_ptr_traits<decltype(Setter)>;
        using getter_traits = fn_ptr_traits<decltype(Getter)>;

        static_assert(not std::is_same<typename setter_traits::arg_type, std::false_type>::value,
            "Column template argument should be a pointer to a class method that sets the column data");
        static_assert(std::is_void<typename setter_traits::return_type>::value,
            "Column template argument should be a pointer to a class method that sets the column data. The return type should be `void`");

        static_assert(std::is_same<typename getter_traits::arg_type, std::false_type>::value,
            "Column template argument should be a pointer to a class method that gets the column data");
        static_assert(not std::is_void<typename getter_traits::return_type>::value,
            "Column template argument should be a pointer to a class method that gets the column data. The return type should not be `void`");

        static_assert(std::is_same<typename getter_traits::return_type, typename setter_traits::arg_type>::value,
                "Column template arguments should be a pointers to class methods that get and set the column data");

        public:
        static constexpr bool public_column = false;
        using member_t = typename setter_traits::arg_type;
        static constexpr sqlite_column_type sql_column_type = member_to_sql_type<member_t>::value;
        using object_class = typename setter_traits::klass;

        using constraints_t = typename __constraint_t_detail::constraints_t<member_t, Constraint...>::type;
        static constexpr bool is_primary_key = any_of<constraint_is_primary_key<Constraint>::value...>;
        static constexpr bool is_auto_inc_column = any_of<constraint_is_primary_key<Constraint>::value...> && sql_column_type == sqlite_column_type::INTEGER;

        static constexpr auto name = column_name;

        static auto& getter(auto& obj) { return (obj.*Getter)(); };
        static void setter(auto& obj, auto arg) { (obj.*Setter)(arg); };

        static std::string constraint_creation_query() {
            return __constraint_t_detail::constraint_creation_query(constraints_t{});
        }
    };

};
