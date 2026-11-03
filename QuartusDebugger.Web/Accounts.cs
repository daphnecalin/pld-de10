using Microsoft.AspNetCore.Identity;
using Microsoft.AspNetCore.Identity.EntityFrameworkCore;
using Microsoft.EntityFrameworkCore;

namespace QuartusDebugger.Web;

public sealed class StudentAccount : IdentityUser
{
    public string FirstName { get; set; } = "";
    public string LastName { get; set; } = "";
}

public sealed class AccountDb(DbContextOptions<AccountDb> options)
    : IdentityDbContext<StudentAccount>(options);

public sealed record Registration(string StudentId, string FirstName, string LastName,
    string Password, string ConfirmPassword);
public sealed record Login(string StudentId, string Password);
