#pragma once

/**
 * @file github_cdn_ca.h
 * @brief CA certificates for GitHub CDN (objects.githubusercontent.com).
 *
 * These certificates are used to maintain secure TLS connections when
 * downloading firmware from GitHub release assets, eliminating the need
 * for setInsecure() which was a security vulnerability.
 *
 * DigiCert Global Root G2 - Valid until 2038
 * This root CA is used by GitHub's CDN infrastructure.
 */

const char *GITHUB_CDN_ROOT_CA =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
    "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
    "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
    "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
    "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
    "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
    "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
    "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
    "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
    "1Yl9PMCcit9F7vYUpuBPE7eSV/jCM0ZJk4saOpXy6fryHkhQPBE5oCXJhKzmvGrB\n"
    "QvSdCBIoNVaw5j7Nuw/ox7VHrylEWq1czfovDAi/lP9Wtg+YdRMnsjxrZVsr2Fum\n"
    "NPWIV4KSEB04zDV4LMovPe9KRxw22dxb8xD9FpNW7CoWbuM7sdPKCH2hLvjP1Pla\n"
    "h86mvIRqMNHclmT7Y1B2G3dL3cPoWjBWVEP0S3LPWPW48Y0as6thhmtJbnzzD6Bk\n"
    "R9JkKps1ekB/S8PB4KqvKJ8vM8iMRoaK/ViYKJksEdB/oyEXl2aodlisMG1oSsjR\n"
    "5nA=\n"
    "-----END CERTIFICATE-----\n";
