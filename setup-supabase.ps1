$ErrorActionPreference = 'Stop'

if (!(Get-Command dotnet -ErrorAction SilentlyContinue) -or !(dotnet --list-sdks)) {
    throw 'Install the .NET 8 SDK, then run this script again.'
}

$webProject = Join-Path $PSScriptRoot 'QuartusDebugger.Web/QuartusDebugger.Web.csproj'
$rootCertificate = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'QuartusDebugger.Web/Certificates/prod-ca-2021.crt')).Path

$databasePassword = [System.Net.NetworkCredential]::new(
    '', (Read-Host 'Team Supabase database password' -AsSecureString)
).Password
if ([string]::IsNullOrWhiteSpace($databasePassword)) {
    throw 'Enter the database password provided by your team.'
}

$storageKey = [System.Net.NetworkCredential]::new(
    '', (Read-Host 'Team Supabase Storage secret key (sb_secret_...)' -AsSecureString)
).Password.Trim()
if (!$storageKey.StartsWith('sb_secret_', [StringComparison]::Ordinal) -or $storageKey.Length -le 10) {
    throw 'Enter a Supabase secret key beginning with sb_secret_.'
}

$connection = [System.Data.Common.DbConnectionStringBuilder]::new()
$connection['Host'] = 'aws-0-us-east-1.pooler.supabase.com'
$connection['Port'] = 5432
$connection['Database'] = 'postgres'
$connection['Username'] = 'postgres.ujcugfcfgtebviisrvxq'
$connection['Password'] = $databasePassword
$connection['SSL Mode'] = 'VerifyFull'
$connection['Root Certificate'] = $rootCertificate
$connection['Maximum Pool Size'] = 5

# Send JSON over stdin so credentials stay out of command arguments and history.
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
@{
    'ConnectionStrings:AppDatabase' = $connection.ConnectionString
    'Supabase:Url' = 'https://ujcugfcfgtebviisrvxq.supabase.co'
    'Supabase:Bucket' = 'submissions'
    'Supabase:SecretKey' = $storageKey
} | ConvertTo-Json -Compress | dotnet user-secrets set --project $webProject | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw 'Could not save the ASP.NET development settings.'
}

Write-Host 'Settings saved. Open PowerShell in QuartusDebugger.Web and run:'
Write-Host 'dotnet run'
