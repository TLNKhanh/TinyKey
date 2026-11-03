# Bind WinUSB to the WCH-Link RISC-V debug interface (VID_1A86&PID_8010&MI_00)
# so libusb / minichlink can talk to it - a scripted alternative to Zadig.
#
# Run as ADMINISTRATOR (right-click PowerShell -> Run as administrator).
# Windows 11 x64 requires a signed driver package; this builds a self-signed
# code-signing cert, creates a catalog for the primitive WinUSB INF and signs it.
# Because the INF only binds the inbox winusb.sys, a self-signed catalog is accepted.

$ErrorActionPreference = "Stop"
$dir = $PSScriptRoot
$inf = Join-Path $dir "wchlink_winusb.inf"
$cat = Join-Path $dir "wchlink_winusb.cat"

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
        ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
  Write-Host "This script must be run as Administrator." -ForegroundColor Red
  exit 1
}

Remove-Item $cat -ErrorAction SilentlyContinue

# 1) self-signed code-signing cert
$cert = New-SelfSignedCertificate -Type CodeSigningCert `
          -Subject "CN=WCHLink WinUSB Installer" `
          -CertStoreLocation "Cert:\LocalMachine\My" `
          -KeyUsage DigitalSignature -KeySpec Signature `
          -NotAfter (Get-Date).AddYears(10) `
          -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3")
Write-Host "cert: $($cert.Thumbprint)"

# 2) trust it
foreach($store in @("Root","TrustedPublisher")){
  $s = New-Object System.Security.Cryptography.X509Certificates.X509Store($store,"LocalMachine")
  $s.Open("ReadWrite"); $s.Add($cert); $s.Close()
  Write-Host "trusted: LocalMachine\$store"
}

# 3) build + sign catalog
New-FileCatalog -Path $dir -CatalogFilePath $cat -CatalogVersion 2 | Out-Null
$sig = Set-AuthenticodeSignature -FilePath $cat -Certificate $cert -HashAlgorithm SHA256
Write-Host "catalog signature: $($sig.Status)"
if ($sig.Status -ne "Valid") { throw "catalog signing failed" }

# 4) stage + install onto the device
pnputil /add-driver $inf /install
pnputil /scan-devices | Out-Null
Write-Host "`nDone. Verify with:  minichlink -i" -ForegroundColor Green
