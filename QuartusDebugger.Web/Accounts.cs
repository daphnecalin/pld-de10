using Microsoft.AspNetCore.Identity;
using Microsoft.AspNetCore.DataProtection.EntityFrameworkCore;
using Microsoft.AspNetCore.Identity.EntityFrameworkCore;
using Microsoft.EntityFrameworkCore;

namespace QuartusDebugger.Web;

public sealed class StudentAccount : IdentityUser
{
    public string FirstName { get; set; } = "";
    public string LastName { get; set; } = "";
}

public sealed class AccountDb(DbContextOptions<AccountDb> options)
    : IdentityDbContext<StudentAccount>(options), IDataProtectionKeyContext
{
    public DbSet<Submission> Submissions => Set<Submission>();
    public DbSet<DataProtectionKey> DataProtectionKeys => Set<DataProtectionKey>();

    protected override void OnModelCreating(ModelBuilder builder)
    {
        base.OnModelCreating(builder);
        builder.HasDefaultSchema("pld");
        var submissions = builder.Entity<Submission>();
        submissions.HasKey(item => item.Id);
        submissions.HasIndex(item => new { item.OwnerUserId, item.UploadedAt });
        submissions.HasIndex(item => item.UploadedAt);
        submissions.HasOne<StudentAccount>().WithMany().HasForeignKey(item => item.OwnerUserId)
            .OnDelete(DeleteBehavior.Restrict);
    }
}

public sealed record Registration(string StudentId, string FirstName, string LastName,
    string Password, string ConfirmPassword);
public sealed record Login(string StudentId, string Password);
