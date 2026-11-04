# Supabase setup

The website stores accounts, submission records, and cookie signing keys in
Supabase PostgreSQL. It stores ZIPs and generated files in a private Supabase
Storage bucket. Archive extraction uses temporary working space, which the
processor cleans up after the request.

## Connect to the existing team project

Install the .NET 8 SDK and open a fresh PowerShell window at the repository root.
Get the team's database password and Storage secret key from supabase. Then run this one-time setup:

```powershell
powershell -NoProfile -ExecutionPolicy RemoteSigned -File .\setup-supabase.ps1
```

The script fills in the team project URL, pooler settings, bucket name, and path
to the included public CA certificate. It prompts for the two secrets and saves
the configuration in ASP.NET User Secrets under your local user profile. User
Secrets are for development and are stored outside Git. Your profile must be
private because the local secrets file is unencrypted.

From the repository root, enter the web project folder and run the site:

```powershell
cd .\QuartusDebugger.Web
dotnet run
```

Open `http://localhost:5080`. The app uses the existing `pld` schema in the
team's  database. Rerun setup
if the credentials rotate or you move the checkout. If you previously set
Supabase environment variables, start from a fresh terminal because environment
variables take precedence over User Secrets.
