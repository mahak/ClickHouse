#pragma once

#include <Parsers/IAST.h>


namespace DB
{
class ASTRolesOrUsersSet;

/** SET ROLE {DEFAULT | NONE | role [,...] | ALL | ALL EXCEPT role [,...]}
  * SET DEFAULT ROLE {NONE | role [,...] | ALL | ALL EXCEPT role [,...]} TO {user|CURRENT_USER} [,...]
  */
class ASTSetRoleQuery : public IAST
{
public:
    enum class Kind : uint8_t
    {
        SET_ROLE,
        SET_ROLE_DEFAULT,
        SET_DEFAULT_ROLE,
    };
    Kind kind = Kind::SET_ROLE;

    std::shared_ptr<ASTRolesOrUsersSet> roles;
    std::shared_ptr<ASTRolesOrUsersSet> to_users;

    String getID(char) const override;
    ASTPtr clone() const override;

    QueryKind getQueryKind() const override { return QueryKind::Set; }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState &, FormatStateStacked) const override;
};

}
