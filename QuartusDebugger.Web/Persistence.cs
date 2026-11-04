using Microsoft.AspNetCore.DataProtection;
using Microsoft.EntityFrameworkCore;
using Npgsql;

namespace QuartusDebugger.Web;

public static class Persistence
{
    public static void Configure(WebApplicationBuilder builder)
    {
        string connection = builder.Configuration.GetConnectionString("AppDatabase") ??
            throw new InvalidOperationException("Set ConnectionStrings:AppDatabase to your Supabase PostgreSQL connection string.");
        var settings = new NpgsqlConnectionStringBuilder(connection) { SslMode = SslMode.VerifyFull };
        builder.Services.AddDbContext<AccountDb>(options => options.UseNpgsql(settings.ConnectionString));
        builder.Services.AddDataProtection().SetApplicationName("QuartusDebugger")
            .PersistKeysToDbContext<AccountDb>();
        SubmissionFiles.Configure(builder);
    }

}
