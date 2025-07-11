#include <Dictionaries/DictionaryFactory.h>
#include <Dictionaries/DictionarySourceFactory.h>

namespace DB
{

class DictionarySourceFactory;

void registerDictionarySourceNull(DictionarySourceFactory & factory);
void registerDictionarySourceFile(DictionarySourceFactory & source_factory);
void registerDictionarySourceMysql(DictionarySourceFactory & source_factory);
void registerDictionarySourceClickHouse(DictionarySourceFactory & source_factory);
void registerDictionarySourceMongoDB(DictionarySourceFactory & source_factory);
void registerDictionarySourceMongoDBPocoLegacy(DictionarySourceFactory & source_factory);
void registerDictionarySourceCassandra(DictionarySourceFactory & source_factory);
void registerDictionarySourceRedis(DictionarySourceFactory & source_factory);
void registerDictionarySourceXDBC(DictionarySourceFactory & source_factory);
void registerDictionarySourceJDBC(DictionarySourceFactory & source_factory);
void registerDictionarySourcePostgreSQL(DictionarySourceFactory & source_factory);
void registerDictionarySourceExecutable(DictionarySourceFactory & source_factory);
void registerDictionarySourceExecutablePool(DictionarySourceFactory & source_factory);
void registerDictionarySourceHTTP(DictionarySourceFactory & source_factory);
void registerDictionarySourceLibrary(DictionarySourceFactory & source_factory);
void registerDictionarySourceYAMLRegExpTree(DictionarySourceFactory & source_factory);

class DictionaryFactory;
void registerDictionaryRangeHashed(DictionaryFactory & factory);
void registerDictionaryComplexKeyHashed(DictionaryFactory & factory);
void registerDictionaryTrie(DictionaryFactory & factory);
void registerDictionaryFlat(DictionaryFactory & factory);
void registerDictionaryRegExpTree(DictionaryFactory & factory);
void registerDictionaryHashed(DictionaryFactory & factory);
void registerDictionaryArrayHashed(DictionaryFactory & factory);
void registerDictionaryCache(DictionaryFactory & factory);
void registerDictionaryPolygon(DictionaryFactory & factory);
void registerDictionaryDirect(DictionaryFactory & factory);


void registerDictionaries()
{
    {
        auto & source_factory = DictionarySourceFactory::instance();
        registerDictionarySourceNull(source_factory);
        registerDictionarySourceFile(source_factory);
        registerDictionarySourceMysql(source_factory);
        registerDictionarySourceClickHouse(source_factory);
        registerDictionarySourceMongoDB(source_factory);
        registerDictionarySourceRedis(source_factory);
        registerDictionarySourceCassandra(source_factory);
        registerDictionarySourceXDBC(source_factory);
        registerDictionarySourceJDBC(source_factory);
        registerDictionarySourcePostgreSQL(source_factory);
        registerDictionarySourceExecutable(source_factory);
        registerDictionarySourceExecutablePool(source_factory);
        registerDictionarySourceHTTP(source_factory);
        registerDictionarySourceLibrary(source_factory);
        registerDictionarySourceYAMLRegExpTree(source_factory);
    }

    {
        auto & factory = DictionaryFactory::instance();
        registerDictionaryRangeHashed(factory);
        registerDictionaryTrie(factory);
        registerDictionaryFlat(factory);
        registerDictionaryRegExpTree(factory);
        registerDictionaryHashed(factory);
        registerDictionaryArrayHashed(factory);
        registerDictionaryCache(factory);
        registerDictionaryPolygon(factory);
        registerDictionaryDirect(factory);
    }
}

}
