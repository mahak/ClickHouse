#include <Interpreters/evaluateConstantExpression.h>
#include <Interpreters/parseColumnsListForTableFunction.h>

#include <Storages/StorageFile.h>
#include <Storages/VirtualColumnUtils.h>
#include <Storages/checkAndGetLiteralArgument.h>

#include <Common/Exception.h>
#include <Formats/FormatFactory.h>
#include <Parsers/ASTFunction.h>
#include <TableFunctions/ITableFunctionFileLike.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
    extern const int BAD_ARGUMENTS;
    extern const int NUMBER_OF_ARGUMENTS_DOESNT_MATCH;
}

void ITableFunctionFileLike::parseFirstArguments(const ASTPtr & arg, const ContextPtr &)
{
    filename = checkAndGetLiteralArgument<String>(arg, "source");
}

std::optional<String> ITableFunctionFileLike::tryGetFormatFromFirstArgument()
{
    return FormatFactory::instance().tryGetFormatFromFileName(filename);
}

bool ITableFunctionFileLike::supportsReadingSubsetOfColumns(const ContextPtr & context)
{
    return format != "auto" && FormatFactory::instance().checkIfFormatSupportsSubsetOfColumns(format, context);
}

NameSet ITableFunctionFileLike::getVirtualsToCheckBeforeUsingStructureHint() const
{
    return VirtualColumnUtils::getVirtualNamesForFileLikeStorage();
}

void ITableFunctionFileLike::parseArguments(const ASTPtr & ast_function, ContextPtr context)
{
    /// Parse args
    ASTs & args_func = ast_function->children;

    if (args_func.size() != 1)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Table function '{}' must have arguments.", getName());

    ASTs & args = args_func.at(0)->children;
    parseArgumentsImpl(args, context);
}

void ITableFunctionFileLike::parseArgumentsImpl(ASTs & args, const ContextPtr & context)
{
    if (args.empty() || args.size() > getMaxNumberOfArguments())
        throw Exception(ErrorCodes::NUMBER_OF_ARGUMENTS_DOESNT_MATCH, "The signature of table function {} shall be the following:\n{}", getName(), getSignature());

    for (auto & arg : args)
        arg = evaluateConstantExpressionOrIdentifierAsLiteral(arg, context);

    parseFirstArguments(args[0], context);

    if (args.size() > 1)
        format = checkAndGetLiteralArgument<String>(args[1], "format");

    if (format == "auto")
    {
        if (auto format_from_first_argument = tryGetFormatFromFirstArgument())
            format = *format_from_first_argument;
    }

    if (args.size() > 2)
    {
        structure = checkAndGetLiteralArgument<String>(args[2], "structure");
        if (structure.empty())
            throw Exception(
                ErrorCodes::BAD_ARGUMENTS,
                "Table structure is empty for table function '{}'. If you want to use automatic schema inference, use 'auto'",
                getName());
    }

    if (args.size() > 3)
        compression_method = checkAndGetLiteralArgument<String>(args[3], "compression_method");
}

StoragePtr ITableFunctionFileLike::executeImpl(const ASTPtr & /*ast_function*/, ContextPtr context, const std::string & table_name, ColumnsDescription /*cached_columns*/, bool is_insert_query) const
{
    ColumnsDescription columns;
    if (structure != "auto")
        columns = parseColumnsListFromString(structure, context);
    else if (!structure_hint.empty())
        columns = structure_hint;

    StoragePtr storage = getStorage(filename, format, columns, context, table_name, compression_method, is_insert_query);
    storage->startup();
    return storage;
}

}
