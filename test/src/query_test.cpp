#include <gtest/gtest.h>
#include <filesystem>
#include "zxorm/zxorm.hpp"
#include "logger.hpp"

using namespace zxorm;

struct Object {
    int id = 0;
    int some_id = 13;
    std::string some_text = "heelllo";
    float some_float = 11.0;
    bool some_bool = false;
    std::optional<float> some_optional;
    std::optional<std::vector<char>> some_optional_buffer;
};

using table_t = Table<"test", Object,
    Column<"id", &Object::id, PrimaryKey<>>,
    Column<"text", &Object::some_text>,
    Column<"float", &Object::some_float>,
    Column<"bool", &Object::some_bool>,
    Column<"some_id", &Object::some_id >,
    Column<"some_optional", &Object::some_optional>,
    Column<"some_optional_buffer", &Object::some_optional_buffer>
        >;

struct OtherObj {
    std::string some_text;
};

using pk_is_text_t = Table<"test2", OtherObj,
    Column<"text", &OtherObj::some_text, PrimaryKey<conflict_t::abort>>>;

using connection_t = Connection<table_t, pk_is_text_t>;

class QueryTest : public ::testing::Test {
    protected:
    void SetUp() override {
        auto created_conn = connection_t::create("test.db", 0, 0, &logger);
        ASSERT_FALSE(created_conn.is_error());

        my_conn = std::make_shared<connection_t>(created_conn.value());

        auto err = my_conn->create_tables(true);
        ASSERT_FALSE(err);
    }

    std::shared_ptr<connection_t> my_conn;

    void TearDown() override {
        my_conn = nullptr;
        std::filesystem::rename("test.db", "test.db.old");
    }
};

TEST_F(QueryTest, FindNothing)
{
    auto result = my_conn->find_record<Object>(1);
    if (result.is_error()) {
        auto err = result.error();
        std::cout << std::string(err) << std::endl;
        ASSERT_FALSE(true);
    } else {
        std::optional<Object> record = std::move(result);
        if (record.has_value()) {
            std::cout << record.value().id << std::endl;
            std::cout << record.value().some_id << std::endl;
        }
        ASSERT_FALSE(record);
    }
}

TEST_F(QueryTest, InsertSomething)
{
    Object obj;

    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_bool = true;

    auto err = my_conn->insert_record(obj);
    if (err) {
        std::cout << std::string(err.value()) << std::endl;
    }
    ASSERT_FALSE(err);
    ASSERT_EQ(obj.id, 1);
}

TEST_F(QueryTest, InsertObjWithoutRowId)
{
    OtherObj obj = { .some_text = "Some text" };

    auto err = my_conn->insert_record(obj);
    if (err) {
        std::cout << std::string(err.value()) << std::endl;
    }

    ASSERT_FALSE(err);
}

TEST_F(QueryTest, FindSomething)
{
    Object obj;
    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_bool = true;

    auto err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);

    auto result = my_conn->find_record<Object>(1);
    if (result.is_error()) {
        auto err = result.error();
        std::cout << std::string(err) << std::endl;
        ASSERT_FALSE(true);
    } else {
        std::optional<Object> record = std::move(result);
        ASSERT_TRUE(record);
        ASSERT_EQ(record.value().id, 1);
        ASSERT_EQ(record.value().some_text, "Some text");
        ASSERT_FLOAT_EQ(record.value().some_float, 3.14);
        ASSERT_TRUE(record.value().some_bool);
        ASSERT_EQ(record.value().some_id, 42);
        ASSERT_FALSE(record.value().some_optional.has_value());
        ASSERT_FALSE(record.value().some_optional_buffer.has_value());
    }
}

TEST_F(QueryTest, FindManyTimes)
{
    std::vector<std::string> text = {
        "hello",
        "there",
        "this",
        "is",
        "text",
    };

    for (size_t i = 0; i < text.size(); i++) {
        Object obj;
        obj.some_text = text[i];
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    for (int id = 1; id <= 5; id++) {
        auto result = my_conn->find_record<Object>(id);
        if (result.is_error()) {
            auto err = result.error();
            std::cout << std::string(err) << std::endl;
        }
        ASSERT_FALSE(result.is_error());
        std::optional<Object> record = std::move(result);
        ASSERT_TRUE(record);
        ASSERT_EQ(record.value().id, id);
        ASSERT_EQ(record.value().some_text, text[id - 1]);
    }
}

TEST_F(QueryTest, FindSomethingWithOptionalsFilled)
{
    Object obj;
    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_optional = 42.333;
    obj.some_optional_buffer = {'y', 'o'};

    auto err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);

    auto result = my_conn->find_record<Object>(1);
    if (result.is_error()) {
        std::cout << std::string(result.error()) << std::endl;
        ASSERT_FALSE(true);
    }

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().id, 1);
    ASSERT_EQ(result.value().some_text, "Some text");
    ASSERT_FLOAT_EQ(result.value().some_float, 3.14);
    ASSERT_FALSE(result.value().some_bool);
    ASSERT_EQ(result.value().some_id, 42);
    ASSERT_TRUE(result.value().some_optional.has_value());
    ASSERT_FLOAT_EQ(result.value().some_optional.value(), 42.333);
    auto expectedBuff = std::vector<char> {'y', 'o'};
    ASSERT_EQ(result.value().some_optional_buffer.value(), expectedBuff);
}

TEST_F(QueryTest, DeleteSomething)
{
    Object obj;
    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_optional = 42.333;
    obj.some_optional_buffer = {'y', 'o'};

    auto err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);
    err = my_conn->delete_record<Object>(1);
    ASSERT_FALSE(err);
    auto result = my_conn->find_record<Object>(1);
    ASSERT_FALSE(result.is_error());
    ASSERT_FALSE(result.has_value());
}

TEST_F(QueryTest, WhereEq)
{
    Object obj;
    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_optional = 42.333;
    obj.some_optional_buffer = {'y', 'o'};

    auto err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);
    auto result = my_conn->select_query<Object>().where(table_t::field_t<"some_id">() == 42).many();
    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    int i = 0;
    for (const auto& result: iter) {
        // we should only see one record
        ASSERT_TRUE(i++ < 1);
        ASSERT_FALSE(result.is_error());

        auto& record = result.value();
        ASSERT_EQ(obj.id, record.id);
        ASSERT_EQ(1, record.id);
        ASSERT_EQ(obj.some_text, record.some_text);
        ASSERT_EQ(obj.some_id, record.some_id);
        ASSERT_EQ(obj.some_float, record.some_float);
        ASSERT_EQ(obj.some_optional, record.some_optional);
        ASSERT_EQ(obj.some_optional_buffer, record.some_optional_buffer);
    }

    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 1);

    auto& record = vec.value()[0];
    ASSERT_EQ(obj.id, record.id);
    ASSERT_EQ(1, record.id);
    ASSERT_EQ(obj.some_text, record.some_text);
    ASSERT_EQ(obj.some_id, record.some_id);
    ASSERT_EQ(obj.some_float, record.some_float);
    ASSERT_EQ(obj.some_optional, record.some_optional);
    ASSERT_EQ(obj.some_optional_buffer, record.some_optional_buffer);
}

TEST_F(QueryTest, WhereFindNothing)
{
    auto result = my_conn->select_query<Object>().where(table_t::field_t<"some_id">() == 42).many();
    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 0);
}

TEST_F(QueryTest, WhereFindMany)
{
    Object obj;
    obj.some_id = 42;

    for (size_t i = 0; i < 4; i++) {
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    auto result = my_conn->select_query<Object>().where(table_t::field_t<"some_id">() == 42).many();
    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 4);
}

TEST_F(QueryTest, WhereEqOrEq)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    auto result = my_conn->select_query<Object>().where(
            table_t::field_t<"some_id">() == 0 ||
            table_t::field_t<"some_id">() == 1 ||
            table_t::field_t<"some_id">() == 2).many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 3);
}

TEST_F(QueryTest, WhereNe)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    auto result = my_conn->select_query<Object>().where(table_t::field_t<"some_id">() != 0).many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 3);
}

TEST_F(QueryTest, WhereNeAndNe)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    auto result = my_conn->select_query<Object>().where(
            table_t::field_t<"some_id">() != 0 &&
            table_t::field_t<"id">() != 2).many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 2);
}

TEST_F(QueryTest, WhereLt)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    auto result = my_conn->select_query<Object>().where(table_t::field_t<"id">() < 2).many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 1);
}

TEST_F(QueryTest, WhereLte)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    auto result = my_conn->select_query<Object>().where(table_t::field_t<"id">() <= 2).many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 2);
}

TEST_F(QueryTest, WhereGt)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    auto result = my_conn->select_query<Object>().where(table_t::field_t<"id">() > 2).many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 2);
}

TEST_F(QueryTest, WhereGte)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    auto result = my_conn->select_query<Object>().where(table_t::field_t<"id">() >= 2).many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 3);
}

TEST_F(QueryTest, WhereLike)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->select_query<Object>().where(table_t::field_t<"text">().like(std::string("hello_"))).many();
    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());
    auto iter = result.value();

    auto vec = iter.to_vector();
    if (vec.is_error()) std::cout << vec.error() << std::endl;
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 4);
}

TEST_F(QueryTest, WhereNotLike)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->select_query<Object>().where(table_t::field_t<"text">().not_like("hello_")).many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 1);
}

TEST_F(QueryTest, WhereGlob)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->select_query<Object>().where(table_t::field_t<"text">().glob("hello*")).many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 4);
}

TEST_F(QueryTest, WhereNotGlob)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));
    obj.some_text = std::string("h5");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->select_query<Object>().where(table_t::field_t<"text">().not_glob("hell*")).many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 1);
}

TEST_F(QueryTest, WhereIn)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    auto search = std::vector<std::string>{"hello1", "hello2"};

    auto result = my_conn->select_query<Object>()
        .where(table_t::field_t<"text">().in(search))
        .many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 2);
}

TEST_F(QueryTest, WhereNotIn)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    auto search = std::array<std::string, 2>{"hello1", "hello2"};

    auto result = my_conn->select_query<Object>()
        .where(table_t::field_t<"text">().not_in(search))
        .many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 2);
}

TEST_F(QueryTest, All)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));
    obj.some_text = std::string("h5");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->select_query<Object>().many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 6);
}

TEST_F(QueryTest, SelectFrom)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));
    obj.some_text = std::string("h5");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->select_query<Select<Object>, From<Object>>().many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 6);
}

TEST_F(QueryTest, SelectWithoutFrom)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));
    obj.some_text = std::string("h5");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->select_query<Select<Object>>().many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 6);
}

TEST_F(QueryTest, OrderDesc)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));
    obj.some_text = std::string("h5");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->select_query<Object>().order_by<table_t::field_t<"id">>(order_t::DESC).many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 6);
    ASSERT_EQ(vec.value()[0].id, 6);
    ASSERT_EQ(vec.value()[1].id, 5);
    ASSERT_EQ(vec.value()[2].id, 4);
    ASSERT_EQ(vec.value()[3].id, 3);
    ASSERT_EQ(vec.value()[4].id, 2);
    ASSERT_EQ(vec.value()[5].id, 1);
}

TEST_F(QueryTest, OrderAscOne)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));
    obj.some_text = std::string("h5");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->select_query<Object>().order_by<table_t::field_t<"id">>(order_t::ASC).one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().id, 1);
}

TEST_F(QueryTest, OrderAscLimit)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));
    obj.some_text = std::string("h5");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->select_query<Object>()
        .order_by<table_t::field_t<"id">>(order_t::DESC)
        .limit(3)
        .many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 3);
    ASSERT_EQ(vec.value()[0].id, 6);
    ASSERT_EQ(vec.value()[1].id, 5);
    ASSERT_EQ(vec.value()[2].id, 4);
}

TEST_F(QueryTest, LimitWithOffset)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));
    obj.some_text = std::string("h5");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->select_query<Object>()
        .limit(3, 3)
        .many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 3);
    ASSERT_EQ(vec.value()[0].id, 4);
    ASSERT_EQ(vec.value()[1].id, 5);
    ASSERT_EQ(vec.value()[2].id, 6);
}

TEST_F(QueryTest, EmptyOne)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));
    obj.some_text = std::string("h5");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->select_query<Object>().where(table_t::field_t<"id">() > 10).one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());
    ASSERT_FALSE(result.has_value());
}

TEST_F(QueryTest, First)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));
    obj.some_text = std::string("h5");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->first<Object>();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().id, 1);
}

TEST_F(QueryTest, Last)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = std::string("helllo4");
    ASSERT_FALSE(my_conn->insert_record(obj));
    obj.some_text = std::string("h5");
    ASSERT_FALSE(my_conn->insert_record(obj));

    auto result = my_conn->last<Object>();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().id, 6);
}

TEST_F(QueryTest, UpdateSomething)
{
    Object obj;

    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_bool = true;

    auto err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);

    obj.some_text = "Some different text";
    err = my_conn->update_record(obj);
    ASSERT_FALSE(err);

    auto result = my_conn->find_record<Object>(obj.id);
    ASSERT_FALSE(result.is_error());
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().some_text, std::string("Some different text"));
}

TEST_F(QueryTest, InsertMany)
{
    std::vector<Object> objects;
    for (int i = 0; i < 200; i++) {
        objects.push_back({
            .some_id = i,
            .some_text = "this is some text" + std::to_string(i),
            .some_float = float(3.14 * i),
            .some_bool = true,
            .some_optional = float(i),
            .some_optional_buffer = std::vector<char> {(char)i, 'b'}
        });
    }

    auto err = my_conn->insert_many_records(objects);

    ASSERT_FALSE(err);

    auto result = my_conn->select_query<Object>().many();
    ASSERT_FALSE(result.is_error());
    auto inserted = result.value().to_vector();
    ASSERT_FALSE(inserted.is_error());
    ASSERT_EQ(200, inserted.value().size());
    for (int i = 0; i < 200; i++) {
        ASSERT_EQ(i, inserted.value()[i].some_id);
    }
}

TEST_F(QueryTest, DeleteWhere)
{
    std::vector<Object> objects;
    for (int i = 0; i < 200; i++) {
        objects.push_back({
            .some_id = i,
            .some_text = "this is some text" + std::to_string(i),
            .some_float = float(3.14 * i),
            .some_bool = true,
            .some_optional = float(i),
            .some_optional_buffer = std::vector<char> {(char)i, 'b'}
        });
    }

    auto err = my_conn->insert_many_records(objects);
    ASSERT_FALSE(err);

    err = my_conn->delete_query<Object>().where(table_t::field_t<"float">() >= (100.0 * 3.14)).exec();
    ASSERT_FALSE(err);

    auto result = my_conn->select_query<Object>().many();
    ASSERT_FALSE(result.is_error());
    auto undeleted = result.value().to_vector();
    ASSERT_FALSE(undeleted.is_error());
    ASSERT_EQ(100, undeleted.value().size());
}

TEST_F(QueryTest, Truncate)
{
    std::vector<Object> objects;
    for (int i = 0; i < 200; i++) {
        objects.push_back({
            .some_id = i,
            .some_text = "this is some text" + std::to_string(i),
            .some_float = float(3.14 * i),
            .some_bool = true,
            .some_optional = float(i),
            .some_optional_buffer = std::vector<char> {(char)i, 'b'}
        });
    }

    auto err = my_conn->insert_many_records(objects);
    ASSERT_FALSE(err);

    err = my_conn->truncate<Object>();
    ASSERT_FALSE(err);

    auto result = my_conn->select_query<Object>().many();
    ASSERT_FALSE(result.is_error());
    auto undeleted = result.value().to_vector();
    ASSERT_FALSE(undeleted.is_error());
    ASSERT_EQ(0, undeleted.value().size());
}

TEST_F(QueryTest, BindStringView) {
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    obj.some_text = "something else";
    auto err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);

    std::string_view search = "hello%";

    auto result = my_conn->select_query<Object>()
        .where(table_t::field_t<"text">().like(search))
        .many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 4);
}

TEST_F(QueryTest, BindStringViewVector)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        auto err = my_conn->insert_record(obj);
        ASSERT_FALSE(err);
    }

    auto search = std::vector<std::string_view>{"hello1", "hello2"};

    auto result = my_conn->select_query<Object>()
        .where(table_t::field_t<"text">().in(search))
        .many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 2);
}

TEST_F(QueryTest, SelectAColumn)
{
    std::vector<Object> objs;
    objs.resize(4);
    auto err = my_conn->insert_many_records(objs);
    ASSERT_FALSE(err);

    auto result = my_conn->select_query<table_t::field_t<"id">>().order_by<table_t::field_t<"id">>(order_t::DESC).one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), 4);
}

TEST_F(QueryTest, SelectSeveralColumns)
{
    Object obj;
    obj.some_text = "yes";
    auto err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);

    auto result = my_conn->select_query<
        Select<table_t::field_t<"id">, table_t::field_t<"text">>
    >().order_by<table_t::field_t<"id">>(order_t::DESC).one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    ASSERT_TRUE(result.has_value());
    int id = std::get<0>(result.value());
    ASSERT_EQ(1, id);

    std::string text = std::get<1>(result.value());
    ASSERT_EQ(text, "yes");
}

TEST_F(QueryTest, ReuseAQuery)
{
    Object obj;
    obj.some_text = "yes";
    auto err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);

    auto query = my_conn->select_query<table_t::field_t<"text">>();
    query.order_by<table_t::field_t<"id">>(order_t::DESC);

    auto result = query.one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    std::string text = result.value();
    ASSERT_EQ(text, "yes");

    obj.id = 0;
    obj.some_text = "nope";
    err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);

    result = query.one();
    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    text = result.value();
    ASSERT_EQ(text, "nope");
}
