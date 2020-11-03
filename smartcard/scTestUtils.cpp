/* ****************************************************************************
 * Copyright 2019-2020 VMware, Inc.  All rights reserved. -- VMware Confidential
 * ****************************************************************************/

/*
 * scTestUtils.cpp --
 *
 *    Implements the Smart Card related test cases
 */

#include <chrono>
#include <direct.h>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <stdio.h>
#include <sstream>
#include <tchar.h>
#include <thread>
#include <vector>
#include <windows.h>
#include <wincrypt.h>
#include <winscard.h>
#include <json/json.h>

#include "logUtils.h"
#include "scTestUtils.h"

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "winscard.lib")

#define _CRT_SECURE_NO_WARNINGS

#ifdef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#undef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#endif

#pragma warning(disable: 4430) // Disable deprecation

static std::vector<SCARD_READERSTATE> GetReaderInfo(SCARDCONTEXT hContext, TCHAR **readerBuf);

template <typename T>
using raii_ptr = std::unique_ptr<T, std::function<void(T *)>>;
#define TIME_ONE_SECOND 1000

/*
 * Retrieving certificates from Smart Card may fail with unstable
 * issue. If NCryptEnumKeys fails, retry a few times.
 */
const int NCRYPT_ENUM_KEYS_RETRY_COUNT = 3;
/*
 * In recent CI test, we found in RedHat7 env, sometimes the 1st call to
 * SCardEstablishContext fails with error code "SCARD_F_INTERNAL_ERROR"
 * but the 2nd try will succeed. Looks there is an issue with 3rd party
 * pcsclite. could also reproduce this with manually running the health
 * check in the VM. Add retrying to avoid such 3rd party program's issue.
 */
const int ESTABLISH_CONTEXT_RETRY_COUNT = 3;


/*
 *-----------------------------------------------------------------------------
 *
 * PCSCStringifyError --
 *
 *      Get the return code message for PC/SC API.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static char *
PCSCStringifyError(LONG returnCode) // IN
{
   static char out[20];
   printf_s(out, sizeof(out), "0x%08X", returnCode);

   return out;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CheckReturnCode --
 *
 *      Check the PC/SC API return code.
 *
 * Results:
 *      true: PC/SC API return success
 *      false: Otherwise
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
CheckReturnCode(char *PCSCAPIDesc, //IN
                LONG returnCode)   //IN
{
   LOG("Command: %s, Return Code: %d\n", PCSCAPIDesc, (int)returnCode);
   if (SCARD_S_SUCCESS != returnCode) {
      LOG_ERROR("%s: %s\n", PCSCAPIDesc, PCSCStringifyError(returnCode));
      return false;
   }
   return true;
}


/*
 *----------------------------------------------------------------------
 *
 * EncryptAndDecryptWithSmartCard --
 *
 *     Encrypt and decrypt test data with smart card's private key
 *
 * Results:
 *     true: Can encrypt and decrypt with PIN successfully.
 *     false: Otherwise
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

bool
EncryptAndDecryptWithSmartCard(std::string pin) // IN
{
   HCRYPTHASH hash = 0;
   DWORD size = 0;
   ALG_ID alg = CALG_SHA_256;
   HCRYPTPROV hProv = 0;
   LPTSTR szCSPName = NULL;
   SCARDCONTEXT context = NULL;
   LONG result = 0;
   DWORD readerLen = SCARD_AUTOALLOCATE;
   LPTSTR readerBuf = NULL;
   bool returnResult = false;
   const size_t bufferLen = MAX_PATH + 11;
   WCHAR defaultContainer[bufferLen];
   DWORD  dwAutoAllocate = SCARD_AUTOALLOCATE;
   LPTSTR szCardName = NULL;
   SCARDCONTEXT hContext;
   BYTE pbBuffer[] = "This is just test data.";
   DWORD dwLen = sizeof(pbBuffer)/sizeof(pbBuffer[0]);
   DWORD dwKeyLen = 0;
   DWORD dwValLen = 0;
   HCRYPTKEY  hKey;

   LOG("Encrypt and decrypt with smart card...");

   result = SCardEstablishContext(SCARD_SCOPE_USER,
                                 NULL,
                                 NULL,
                                 &hContext);
   if (result != SCARD_S_SUCCESS) {
      LOG_ERROR("Fail to establish context.");
      return false;
   }

   std::vector<SCARD_READERSTATE> rgscState =
      GetReaderInfo(hContext, &readerBuf);
   if (rgscState.size() < 1) {
      LOG_ERROR("Fail to get reader. \n");
      goto CLEAN_UP;
   }

   LOG("List smart cards...");
   // Only select first card from reader for testing
   result = SCardListCards(hContext,
                          rgscState[0].rgbAtr,
                          NULL,
                          0,
                          (LPTSTR)&szCardName,
                          &dwAutoAllocate);
   if (result != SCARD_S_SUCCESS) {
      LOG_ERROR("Unable to list cards in reader.");
      goto CLEAN_UP;
   } else {
      LOG("Card Name: %S", szCardName);
   }

   /*
    * szCardName could be a multi-string, but it doesn't matter because all
    * we need the card name for is to retrieve the CSP and we can assume that
    * each card in a reader uses the same CSP.
    */
   dwAutoAllocate = SCARD_AUTOALLOCATE;
   LOG("SCardGetCardTypeProviderName...");
   result = SCardGetCardTypeProviderName(hContext,
                                         szCardName,
                                         SCARD_PROVIDER_CSP,
                                         (LPTSTR)&szCSPName,
                                         &dwAutoAllocate);
   if (result != SCARD_S_SUCCESS) {
      LOG_ERROR("Unable to retrieve CSP for smart card %S", szCardName);
      return returnResult;
   } else {
      LOG("CSP name: %S", szCSPName);
   }

   // zero the buffer out first
   memset(defaultContainer, 0, sizeof(wchar_t) * bufferLen);
   // only write up to bufferLen - 1, i.e. ensure the last character is left zero
   _snwprintf(defaultContainer,
              bufferLen - 1,
              L"\\\\.\\%s\\",
              rgscState[0].szReader);
   LOG("Container: %S\n", defaultContainer);

   LOG("Acquire card context...");
   if (!CryptAcquireContext(&hProv, defaultContainer, szCSPName, PROV_RSA_FULL,
      CRYPT_SILENT | CRYPT_MACHINE_KEYSET)) {
      LOG_ERROR("Fail to acquire context. Error: %d", GetLastError());
      goto CLEAN_UP;
   }

   if (!CryptSetProvParam(hProv, PP_KEYEXCHANGE_PIN, LPBYTE(pin.c_str()), 0)) {
      LOG_ERROR("Fail to set PIN. "
                "Please check the PIN and smart card is inserted.");
      goto CLEAN_UP;
   }

   if (!CryptGetUserKey(hProv, AT_KEYEXCHANGE, &hKey)) {
      LOG_ERROR("Fail to get user key.");
       goto CLEAN_UP;
   }

   dwValLen = sizeof(DWORD);
   if (!CryptGetKeyParam(hKey,
                         KP_KEYLEN,
                         (LPBYTE) &dwKeyLen,
                         &dwValLen,
                         0)) {
      LOG_ERROR("CryptGetKeyParam failed\n");
      goto CLEAN_UP;
   }

   // tranform to bytes length
   dwKeyLen = dwKeyLen / 8;
   if (!CryptEncrypt(hKey,
                    NULL,
                    true,
                    0,
                    pbBuffer,
                    &dwLen,
                    dwKeyLen)) {
      LOG_ERROR("Fail to encrypt data");
      goto CLEAN_UP;
   } else {
      LOG("Crypt data successfully.");
   }

   if(!CryptDecrypt(hKey,
                    NULL,
                    true,
                    0,
                    pbBuffer,
                    &dwLen)) {
      LOG_ERROR("Fail to decrypt data");
      goto CLEAN_UP;
   } else {
      LOG("Decrypt data successfully.");
      returnResult = true;
   }

CLEAN_UP:
   if (readerBuf) {
      SCardFreeMemory(hContext, readerBuf);
      readerBuf = NULL;
   }
   if (szCSPName) {
      SCardFreeMemory(hContext, szCSPName);
      szCSPName= NULL;
   }
   if (hProv) {
      CryptReleaseContext(hProv, 0);
      hProv = NULL;
   }
   SCardReleaseContext(hContext);

   return returnResult;
}


/*
 *----------------------------------------------------------------------
 *
 * SignWithSmartCard --
 *
 *     Sign with smart card's private key
 *
 * Results:
 *     true: Can sign with PIN successfully.
 *     false: Otherwise
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

bool
SignWithSmartCard(std::string pin) // IN
{
   HCRYPTHASH hash = 0;
   DWORD size = 0;
   std::vector<BYTE> signature;
   ALG_ID alg = CALG_SHA_256;
   HCRYPTPROV hProv = 0;
   LPTSTR szCSPName = NULL;
   SCARDCONTEXT hContext = NULL;
   LONG result = 0;
   DWORD readerLen = SCARD_AUTOALLOCATE;
   LPTSTR readerBuf = NULL;
   bool signResult = false;
   const size_t bufferLen = MAX_PATH + 11;
   WCHAR defaultContainer[bufferLen];
   DWORD  dwAutoAllocate = SCARD_AUTOALLOCATE;
   LPTSTR szCardName = NULL;

   LOG("Sign with smart card...");

   result = SCardEstablishContext(SCARD_SCOPE_USER,
                                  NULL,
                                  NULL,
                                  &hContext);
   if (result != SCARD_S_SUCCESS) {
      LOG_ERROR("Fail to establish context.");
      return false;
   }

   std::vector<SCARD_READERSTATE> rgscState =
      GetReaderInfo(hContext, &readerBuf);
   if (rgscState.size() < 1) {
      LOG_ERROR("Fail to get reader. \n");
      goto CLEAN_UP;
   }

   LOG("List smart cards...");
   // Only select first card from reader for testing
   result = SCardListCards(hContext,
                           rgscState[0].rgbAtr,
                           NULL,
                           0,
                           (LPTSTR)&szCardName,
                           &dwAutoAllocate);
   if (result != SCARD_S_SUCCESS) {
      LOG_ERROR("Unable to list cards in reader.");
      goto CLEAN_UP;
   } else {
      LOG("Card Name: %S", szCardName);
   }

   /*
    * szCardName could be a multi-string, but it doesn't matter because all
    * we need the card name for is to retrieve the CSP and we can assume that
    * each card in a reader uses the same CSP.
    */
   dwAutoAllocate = SCARD_AUTOALLOCATE;
   LOG("SCardGetCardTypeProviderName...");
   result = SCardGetCardTypeProviderName(hContext,
                                         szCardName,
                                         SCARD_PROVIDER_CSP,
                                         (LPTSTR)&szCSPName,
                                         &dwAutoAllocate);
   if (result != SCARD_S_SUCCESS) {
      LOG_ERROR("Unable to retrieve CSP for smart card %S", szCardName);
      return false;
   } else {
      LOG("CSP name: %S", szCSPName);
   }

   // zero the buffer out first
   memset(defaultContainer, 0, sizeof(wchar_t) * bufferLen);
   // only write up to bufferLen - 1, i.e. ensure the last character is left zero
   _snwprintf(defaultContainer,
              bufferLen - 1,
              L"\\\\.\\%s\\",
              rgscState[0].szReader);
   LOG("Container: %S\n", defaultContainer);

   LOG("Acquire card context...");
   if (!CryptAcquireContext(&hProv, defaultContainer, szCSPName, PROV_RSA_FULL,
      CRYPT_SILENT | CRYPT_MACHINE_KEYSET)) {
      LOG_ERROR("Fail to acquire context.");
      goto CLEAN_UP;
   }

   if (!CryptSetProvParam(hProv, PP_KEYEXCHANGE_PIN, LPBYTE(pin.c_str()), 0)) {
      LOG_ERROR("Fail to set PIN."
                "Please check the PIN and smart card is inserted.");
      goto CLEAN_UP;
   }

   HCRYPTKEY  privateKey;
   if (CryptGetUserKey(hProv, AT_KEYEXCHANGE, &privateKey)) {
      LOG("Get private key successfully.\n");
   } else {
      LOG_ERROR("Fail to get user key.");
      goto CLEAN_UP;
   }

   if (!CryptCreateHash(hProv, alg, 0, 0, &hash)) {
      LOG_ERROR("Fail to create hash.\n");
      goto CLEAN_UP;
   }

   if (!CryptSetHashParam(hash, HP_HASHVAL, rgscState[0].rgbAtr, 0)) {
      CryptDestroyHash(hash);
      LOG_ERROR("Failed to set hash param");
      goto CLEAN_UP;
   }

   if (!CryptSignHash(hash, AT_KEYEXCHANGE, nullptr, 0, nullptr, &size)) {
      LOG_ERROR("Fail to get sign size.\n");
      CryptDestroyHash(hash);
      goto CLEAN_UP;
   }

   if (size > 0) {
      signature.resize(size);
      if (!CryptSignHash(hash,
                         AT_KEYEXCHANGE,
                         nullptr,
                         0,
                         signature.data(),
                         &size)) {
         LOG_ERROR("Fail to sign hash.");
         goto CLEAN_UP;
      } else {
         LOG("Sign the data successfully.\n");
         signResult = true;
      }

      std::reverse(signature.begin(), signature.end());
      CryptDestroyHash(hash);
   } else {
      LOG("The sign size length is 0.");
   }

CLEAN_UP:
   if (readerBuf) {
      SCardFreeMemory(hContext, readerBuf);
      readerBuf = NULL;
   }
   if (szCSPName) {
      SCardFreeMemory(hContext, szCSPName);
      szCSPName= NULL;
   }
   if (hProv) {
      CryptReleaseContext(hProv, 0);
      hProv = NULL;
   }
   SCardReleaseContext(hContext);

   return signResult;
}


/*
 *----------------------------------------------------------------------
 *
 * RetrieveCertificatesFromSmartCard --
 *
 *     Retrieve certificate keys from Smart Card
 *
 * Results:
 *     true: Can retrieve keys successfully.
 *     false: Otherwise
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

bool
RetrieveCertificatesFromSmartCard()
{
   long status = -1;
   LOG("Retrieving certificates from smart card ...");
   NCRYPT_PROV_HANDLE hProvider = NULL;
   NCryptKeyName *pKeyName = nullptr;
   PVOID pState = nullptr;
   int count = 1;
   int retryCount = 0;
   bool result = false;

   LOG("Health check for smart card...");
   HealthCheckForSmartCard();

   LOG("Opening storage provider...");
   if (NCryptOpenStorageProvider(&hProvider,
                                 MS_SMART_CARD_KEY_STORAGE_PROVIDER, 0)
       != ERROR_SUCCESS) {
      LOG_ERROR("Fail to open storage.");
      return result;
   }

   // Navigate all smart card's certificates
   LOG("Navigate all smart card certificates...");
   do {
      status = NCryptEnumKeys(hProvider,
                              NULL,
                              &pKeyName,
                              &pState,
                              0);

      // No more keys to be enumerated
      if (status == NTE_NO_MORE_ITEMS) {
         break;
      }

      // If error occurs to enumerate the current key, have a retry
      if (status != ERROR_SUCCESS) {
         LOG_ERROR("NCryptEnumKeys: 0x%x. " \
                   "Retry enumerating keys stored by the provider...", status);
         retryCount++;
         continue;
      }

      // Reset the retryCount for new key
      retryCount = 0;

      LOG("Retrieve %d certificate in the smart card.", count);
      count++;

      NCRYPT_KEY_HANDLE keyHandle = NULL;
      auto lReturn = NCryptOpenKey(hProvider,
                                   &keyHandle,
                                   pKeyName->pszName,
                                   pKeyName->dwLegacyKeySpec,
                                   0);

      LOG("Key Name: %S\n", pKeyName->pszName);

      if (keyHandle) {
         NCryptFreeObject(keyHandle);
      }

      result = true;

   // Break loop when error still occurs after max retry times
   } while (retryCount < NCRYPT_ENUM_KEYS_RETRY_COUNT);

   if (!result) {
      LOG_ERROR("Failed to retrieve certs.");
   }

   if (hProvider) {
      NCryptFreeObject(hProvider);
   }

   if (pKeyName) {
      NCryptFreeBuffer(pKeyName);
      pKeyName = nullptr;
   }

   if (pState) {
      NCryptFreeBuffer(pState);
      pState = nullptr;
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * ImportAndDisplayAllCertsInMyStore --
 *
 *     Display all certificates in MY certificate store
 *
 * Results:
 *     true: Can retrieve certificates successfully.
 *     false: Otherwise
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

bool
ImportAndDisplayAllCertsInMyStore()
{
   bool result = ImportCertificatesToMYStore();
   result = result && DisplayAllCertificatesInMyStore();
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * DisplayAllCertificateInMyStore --
 *
 *     Display all certificates in MY certificate store
 *
 * Results:
 *     true: Can retrieve certificates successfully.
 *     false: Otherwise
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

bool
DisplayAllCertificatesInMyStore()
{
   HCERTSTORE hCertStore;
   PCCERT_CONTEXT pCertContext;
   bool result = false;
   LPTSTR pszName;
   DWORD cbSize;
   int i = 1;

   if (!(hCertStore = CertOpenStore(CERT_STORE_PROV_SYSTEM,
                                    PKCS_7_ASN_ENCODING | X509_ASN_ENCODING,
                                    NULL,
                                    CERT_SYSTEM_STORE_CURRENT_USER,
                                    L"MY"))) {
      LOG_ERROR("The MY system store fail to open.");
      return result;
   }

   pCertContext = NULL;
   while (pCertContext = CertEnumCertificatesInStore(hCertStore,
                                                     pCertContext)) {
      LOG("=====================Certificate %d====================", i++);
      if (!(cbSize = CertGetNameString(pCertContext,
                                       CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                       0,
                                       NULL,
                                       NULL,
                                       0))) {
         LOG_ERROR("CertGetNameString failed.");
         goto CLEAN_UP;
      }

      if (!(pszName = (LPTSTR)malloc(cbSize * sizeof(TCHAR)))) {
         LOG_ERROR("Memory allocation failed.");
         goto CLEAN_UP;
      }

      if (CertGetNameString(pCertContext,
                            CERT_NAME_SIMPLE_DISPLAY_TYPE,
                            0,
                            NULL,
                            pszName,
                            cbSize)) {
         LOG("\nSubject: %S.\n", pszName);
         free(pszName);
      } else {
         LOG_ERROR("CertGetName failed.");
         goto CLEAN_UP;
      }

      // Get the name of Issuer of the certificate.
      if (!(cbSize = CertGetNameString(pCertContext,
                                       CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                       CERT_NAME_ISSUER_FLAG,
                                       NULL,
                                       NULL,
                                       0))) {
         LOG_ERROR("CertGetNameString failed.");
         goto CLEAN_UP;
      }

      if (!(pszName = (LPTSTR)malloc(cbSize * sizeof(TCHAR)))) {
         LOG_ERROR("Memory allocation failed.");
         goto CLEAN_UP;
      }

      if (CertGetNameString(pCertContext,
                            CERT_NAME_SIMPLE_DISPLAY_TYPE,
                            CERT_NAME_ISSUER_FLAG,
                            NULL,
                            pszName,
                            cbSize)) {
         LOG("Issuer: %S.\n", pszName);
         result = true;
         free(pszName);
      } else {
         LOG_ERROR("CertGetNameString failed.");
         goto CLEAN_UP;
      }
   } // End of while loop

CLEAN_UP:
   CertCloseStore(hCertStore, CERT_CLOSE_STORE_CHECK_FLAG);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * ImportCertificatesToMYStore --
 *
 *     Import all certificates from Smart Card to MY certificate store
 *
 * Results:
 *     true: Can import successfully.
 *     false: Otherwise
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

bool
ImportCertificatesToMYStore()
{
   int status = -1;
   HCERTSTORE hCertStore = NULL;
   NCRYPT_PROV_HANDLE provider = NULL;
   NCryptKeyName *pKeyName = nullptr;
   PVOID pState = nullptr;
   BYTE *cert = NULL;
   PCCERT_CONTEXT  pCertContext;
   DWORD size = 0;
   int count = 1;
   bool result = false;

   LOG("Import certificates to MY store ...");

   raii_ptr<NCRYPT_PROV_HANDLE> hProvider(&provider,
      [](NCRYPT_PROV_HANDLE *handle) {
      if (handle) {
         NCryptFreeObject(*handle);
      }
   });

   if (!(hCertStore = CertOpenStore(CERT_STORE_PROV_SYSTEM,
                                    PKCS_7_ASN_ENCODING | X509_ASN_ENCODING,
                                    NULL,
                                    CERT_SYSTEM_STORE_CURRENT_USER,
                                    L"MY"))) {
      LOG_ERROR("The MY system store fail to open.");
      return result;
   }

   LOG("Opening storage provider...");
   if (NCryptOpenStorageProvider(hProvider.get(),
                                 MS_SMART_CARD_KEY_STORAGE_PROVIDER, 0)
       != ERROR_SUCCESS) {
      LOG_ERROR("Fail to open storage.");
      return result;
   }

   // Navigate all smart card's certificates
   LOG("Navigate all smart card certificates...");
   while (NCryptEnumKeys(*hProvider,
                         0,
                         &pKeyName,
                         &pState,
                         0)
          == ERROR_SUCCESS) {

      LOG("Retreive %d certificate from the smart card...", count++);
      NCRYPT_KEY_HANDLE key = NULL;
      raii_ptr<NCRYPT_KEY_HANDLE> hKey(&key, [](NCRYPT_KEY_HANDLE *handle) {
         if (handle) {
            NCryptFreeObject(*handle);
         }
      });
      auto status = NCryptOpenKey(*hProvider,
                                  hKey.get(),
                                  pKeyName->pszName,
                                  pKeyName->dwLegacyKeySpec,
                                  0);
      // Get certificate
      NCryptGetProperty(*hKey, NCRYPT_CERTIFICATE_PROPERTY, nullptr, 0, &size, 0);
      cert = new BYTE[size];
      NCryptGetProperty(*hKey, NCRYPT_CERTIFICATE_PROPERTY, cert, size, &size, 0);

      if (pCertContext = CertCreateCertificateContext(X509_ASN_ENCODING,
                                                      cert,
                                                      size)) {
         LOG("Create certificate %S context successfully.\n", pKeyName->pszName);
      } else {
         LOG_ERROR("Fail to create certificate %S context.", pKeyName->pszName);
         result = true;
         goto CLEAN_UP;
      }

      // Add the certificate from the My store to the new memory store.
      if (CertAddCertificateContextToStore(hCertStore,
                                           pCertContext,
                                           CERT_STORE_ADD_REPLACE_EXISTING,
                                           NULL)) {
         LOG("The certificate %S is imported to MY store successfully.", pKeyName->pszName);
      } else {
         LOG_ERROR("Could not add %S certificate to MY store.", pKeyName->pszName);
         result = false;
         goto CLEAN_UP;
      }

      if (cert) {
         free(cert);
         cert = NULL;
      }
      if (pCertContext) {
          CertFreeCertificateContext(pCertContext);
          pCertContext = NULL;
      }
      result = true;
   }

CLEAN_UP:
   if (cert) {
      free(cert);
      cert = NULL;
   }
   if (pCertContext) {
         CertFreeCertificateContext(pCertContext);
         pCertContext = NULL;
   }
   if (hCertStore) {
      CertCloseStore(hCertStore, CERT_CLOSE_STORE_CHECK_FLAG);
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * GetCardATR --
 *
 *     Get the smart card's ATR
 *
 * Results:
 *     The ATR string
 *
 * Side Effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

std::string
GetCardATR()
{
   HCRYPTHASH hash = 0;
   DWORD size = 0;
   std::vector<BYTE> signature;
   ALG_ID alg = CALG_SHA_256;
   HCRYPTPROV hProv = 0;
   DWORD dwAutoAllocate;
   SCARDCONTEXT hContext = NULL;
   LPBYTE pbAttr = NULL;
   DWORD attrLen = 0;
   DWORD readerLen = SCARD_AUTOALLOCATE;
   LPTSTR readerBuf = NULL;
   LONG lReturn = -1;
   dwAutoAllocate = SCARD_AUTOALLOCATE;
   SCARDHANDLE hCard = NULL;
   DWORD dwActiveProtocol;
   std::stringstream ss;
   std::string atr;

   lReturn = SCardEstablishContext(SCARD_SCOPE_USER,
                                   NULL,
                                   NULL,
                                   &hContext);
   if (lReturn != SCARD_S_SUCCESS) {
      LOG_ERROR("Fail to establish context.");
      return false;
   }

   std::vector<SCARD_READERSTATE> rgscState =
      GetReaderInfo(hContext, &readerBuf);
   if (rgscState.size() < 1) {
      LOG_ERROR("Fail to get reader. \n");
      return atr;
   }

   // Retrieve the card name, so we can know what CSP the reader uses.
   lReturn = SCardConnect(hContext,
                          rgscState[0].szReader,
                          SCARD_SHARE_SHARED,
                          SCARD_PROTOCOL_T0|SCARD_PROTOCOL_T1,
                          &hCard,
                          &dwActiveProtocol);
   if (lReturn != SCARD_S_SUCCESS) {
      LOG_ERROR("Fail to call SCardConnect, error: %d", GetLastError());
      goto CLEAN_UP;
   } else {
      attrLen = SCARD_AUTOALLOCATE;
      lReturn = SCardGetAttrib(hCard,
                               SCARD_ATTR_ATR_STRING,
                               (LPBYTE)&pbAttr,
                               &attrLen);
      LOG("ATR Length: %d\n", attrLen);
      if (lReturn != SCARD_S_SUCCESS) {
         LOG_ERROR("Fail to call SCardGetAttrib");
         goto CLEAN_UP;
      } else {
         ss << std::hex;
         for (int i = 0; i < (int)attrLen; ++i) {
            ss << std::setw(2) << std::setfill('0') << (int)*(pbAttr+i);
         }
         atr = ss.str();
         LOG("ATR: %s", atr.c_str());
         LOG("Get ATR successfully.");
      }
      SCardFreeMemory(hContext, pbAttr);
   }

CLEAN_UP:
   if (hCard) {
      SCardDisconnect(hCard, SCARD_LEAVE_CARD);
   }
   SCardReleaseContext(hContext);

   return atr;
}


/*
 *----------------------------------------------------------------------
 *
 * GetReaderInfo --
 *
 *     Get all reader info vector
 *
 * Results:
 *     The vector to store reader info
 *
 * Side Effects:
 *     Release readerBuf memory after calling this funciton.
 *
 *----------------------------------------------------------------------
 */

static std::vector<SCARD_READERSTATE>
GetReaderInfo(SCARDCONTEXT hContext, // IN
              TCHAR **readerBuf)     // OUT
{
   std::vector<SCARD_READERSTATE> rgscState;
   LONG result = -1;
   DWORD readerLen = SCARD_AUTOALLOCATE;

   result = SCardListReaders(hContext,
                             SCARD_DEFAULT_READERS,
                             (LPTSTR)readerBuf,
                             &readerLen);
   if (result != SCARD_S_SUCCESS) {
      LOG_ERROR("Fail to list readers.");
      return rgscState;
   }

   TCHAR *szRdr = *readerBuf;
   for (DWORD dwI = 0; dwI < MAXIMUM_SMARTCARD_READERS; dwI++) {
      if (*szRdr == 0) {
         break;
      }

      SCARD_READERSTATE state = { 0 };
      state.szReader = szRdr;
      state.dwCurrentState = SCARD_STATE_UNAWARE;
      rgscState.push_back(state);
      szRdr += _tcslen(szRdr) + 1;
   }

   if (!rgscState.empty()) {
      /*
       * Call SCardGetStatusChange as a way of asking Windows for the
       * current state of all cards, which will be stored in dwEventState.
       * After that, we will call SCardGetStatusChange and it will actually
       * wait.
       */
      SCardGetStatusChange(hContext,
                           1,
                           &rgscState[0],
                           (DWORD)rgscState.size());
      for (size_t i = 0; i < rgscState.size(); i++) {
         rgscState[i].dwCurrentState = rgscState[i].dwEventState;
      }
   }

   return rgscState;
}


/*
 *----------------------------------------------------------------------
 *
 * WaitCardReady --
 *
 *     Wait until smart card reader and card is ready
 *
 *     timeout: Specify how long it needs to wait until the card is ready.
 *
 * Results:
 *     true: The card is ready.
 *     false: Otherwise
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

bool
WaitCardReady(int timeout) // IN
{
   int               ret = 0;
   HRESULT           hr = S_OK;
   LPTSTR            szReaders = NULL, szRdr = NULL;
   DWORD             cchReaders = SCARD_AUTOALLOCATE;
   DWORD             dwI, dwRdrCount;
   SCARD_READERSTATE rgscState[MAXIMUM_SMARTCARD_READERS];
   TCHAR             szCard[MAX_PATH * 64];
   DWORD              dwCardLen = sizeof(szCard) / sizeof(TCHAR);
   SCARDCONTEXT      hSC;
   LONG              lReturn = -1;
   int               elapsedTime = 1;
   bool              isReady = false;
   time_t            startTime;
   time_t            curTime;

   LOG("Check the smart card is ready...");
   time(&startTime);
   time(&curTime);
   // wait until establish a context.
   while(difftime(curTime, startTime) < timeout &&
      SCARD_S_SUCCESS != SCardEstablishContext(SCARD_SCOPE_USER,
                                               NULL,
                                               NULL,
                                               &hSC)) {
      LOG("Wait until it can establish the card context.");

      Sleep(TIME_ONE_SECOND);
      time(&curTime);
   }

   if (difftime(curTime, startTime) > timeout) {
      LOG("ERROR: Fail to establish card context");
      goto CLEAN_UP;
   }

   // Wait until the reader is ready
   time(&curTime);
   while(difftime(curTime, startTime) < timeout &&
       SCARD_S_SUCCESS != SCardListReaders(hSC,
                                           NULL,
                                           (LPTSTR)&szReaders,
                                           &cchReaders)) {
      LOG("Wait until the ready is ready.");
      Sleep(TIME_ONE_SECOND);
      time(&curTime);
   }

   if (difftime(curTime, startTime) > timeout) {
      LOG_ERROR("Fail to list readers.");
      goto CLEAN_UP;
   }

   if (SCARD_S_SUCCESS != lReturn) {
      LOG_ERROR("Fail to list readers.");
      goto CLEAN_UP;
   }

   // Place the readers into the state array.
   szRdr = szReaders;
   for (dwI = 0; dwI < MAXIMUM_SMARTCARD_READERS; dwI++) {
      if (0 == *szRdr)
         break;
      rgscState[dwI].szReader = szRdr;
      LOG("Reader Name: %S", szRdr);
      rgscState[dwI].dwCurrentState = SCARD_STATE_UNAWARE;
      szRdr += lstrlen(szRdr) + 1;
   }
   dwRdrCount = dwI;

   if (0 == dwRdrCount) {
      LOG_ERROR("There is no available reader.");
      goto CLEAN_UP;
   }

   LOG("Navigate all cards...");
   // If any readers are available, proceed.
   time(&curTime);
   while(difftime(curTime, startTime) < timeout) {
      // Card not found yet; wait until there is a change.
      lReturn = SCardGetStatusChange(hSC,
         1000, // time wait
         rgscState,
         dwRdrCount);
      int state = rgscState[0].dwEventState & SCARD_STATE_PRESENT;
      if (state == 0) {
         Sleep(TIME_ONE_SECOND);
         time(&curTime);
         if (elapsedTime >= timeout) {
            LOG("ERROR: Cannot wait for card inserted before timeout.");
            goto CLEAN_UP;
         } else {
            LOG("The card is not ready. Still waiting... ");
            continue;
         }
      }

      isReady = true;
      LOG("The card is ready.");
      break;
   }

CLEAN_UP:
   if (hSC) {
      SCardReleaseContext(hSC);
   }

   return isReady;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PerformAPDUTest --
 *
 *      Send APDU command to Smart Card and check it can return response
 *
 * Results:
 *      true: Pass the APDU command test
 *      false: Otherwise
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
PerformAPDUTest()
{
   long lReturn = -1;
   SCARDCONTEXT hContext = NULL;
   int retryCount = 0;
   LPTSTR readerBuf = NULL;
   SCARDHANDLE hCard = 0;
   DWORD dwActiveProtocol;
   // Select file identifier command
   BYTE pbSendBuffer[] = { 0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };
   BYTE pbRecvBuffer[10];
   DWORD dwSendLength = sizeof(pbSendBuffer);
   DWORD dwRecvLength = sizeof(pbRecvBuffer);
   const SCARD_IO_REQUEST *pioSendPci;
   bool isSuccess = false;
   bool result = false;

   LOG("Perform APDU Test...");
   do {
      lReturn = SCardEstablishContext(SCARD_SCOPE_USER,
                                      NULL,
                                      NULL,
                                      &hContext);
   } while (SCARD_S_SUCCESS != lReturn &&
            retryCount < ESTABLISH_CONTEXT_RETRY_COUNT);

   if (lReturn != SCARD_S_SUCCESS) {
      LOG_ERROR("Fail to establish context.");
      return false;
   }

   std::vector<SCARD_READERSTATE> rgscStateLst =
      GetReaderInfo(hContext, &readerBuf);
   if (rgscStateLst.size() < 1) {
      LOG_ERROR("Fail to get reader.");
      goto CLEAN_UP;
   }

   for (unsigned int i = 0; i < rgscStateLst.size(); i++) {
      if (SCARD_STATE_PRESENT !=
          (rgscStateLst[i].dwEventState & SCARD_STATE_PRESENT)) {
         LOG_ERROR("Skip to test card %S which is not inserted.",
                   rgscStateLst[i].szReader);
         continue;
      }

      lReturn = SCardConnect(hContext,
                             rgscStateLst[i].szReader,
                             SCARD_SHARE_SHARED,
                             SCARD_PROTOCOL_T0|SCARD_PROTOCOL_T1,
                             &hCard,
                             &dwActiveProtocol);
      if (!CheckReturnCode("SCardConnect", lReturn)) {
         goto CLEAN_UP;
      }

      switch(dwActiveProtocol) {
         case SCARD_PROTOCOL_T0:
            pioSendPci = SCARD_PCI_T0;
            break;
         case SCARD_PROTOCOL_T1:
            pioSendPci = SCARD_PCI_T1;
            break;
         default:
            LOG_ERROR("Unknown protocol %d", (int)dwActiveProtocol);
            goto CLEAN_UP;
      }

      // Begin transaction
      lReturn = SCardBeginTransaction(hCard);
      if (!CheckReturnCode("SCardBeginTransaction", lReturn)) {
         goto CLEAN_UP;
      }

      //Send APDU command
      for (DWORD i=0; i < dwSendLength; i++) {
         printf("%02X ", pbSendBuffer[i]);
      }
      lReturn = SCardTransmit(hCard,
                              pioSendPci,
                              pbSendBuffer,
                              dwSendLength,
                              NULL,
                              pbRecvBuffer,
                              &dwRecvLength);
      if (!CheckReturnCode("SCardTransmit", lReturn)) {
         goto CLEAN_UP;
      }

      //Received response for SCardTransmit
      for (DWORD i=0; i < dwRecvLength; i++) {
         printf("%02X ", pbRecvBuffer[i]);
      }

      SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
      SCardDisconnect(hCard, SCARD_LEAVE_CARD);
      hCard = 0;

      result = true;
      LOG("%S: Can operate with Smart Card successfully.", rgscStateLst[i].szReader);
      break;
   }

CLEAN_UP:
   if (hCard) {
      SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
      SCardDisconnect(hCard, SCARD_LEAVE_CARD);
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HealthCheckForSmartCard --
 *
 *      Check whether Smart Card's status is healthy
 *
 * Results:
 *      true: Pass the Smart Card health check
 *      false: Otherwise
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
HealthCheckForSmartCard()
{
   bool result = false;

   result = PerformAPDUTest();
   if (!result) {
      LOG_ERROR("Health check: APDU test failed.");
      return result;
   }

   return result;
}


int
WideCharToMultiByte(const wchar_t* srcString, // IN
   char** destString)        // OUT
{
   int ret = ERROR_SUCCESS;
   int destLen = ::WideCharToMultiByte(CP_UTF8, 0, srcString, -1, NULL, 0, NULL, NULL);
   if (destLen == 0) {
      ret = GetLastError();
      goto Exit;
   }

   (*destString) = (char*)(calloc(destLen, sizeof(char)));
   if (NULL == (*destString)) {
      ret = ERROR_OUTOFMEMORY;
      goto Exit;
   }

   if (!::WideCharToMultiByte(CP_UTF8, 0, srcString, -1,
      (*destString), destLen, NULL, NULL)) {
      ret = GetLastError();
      free(*destString);
      goto Exit;
   }

Exit:
   return ret;
}



bool
VerifySmartCard(std::string pin,   // IN
   Json::Value& root) // OUT
{
   HCRYPTHASH hash = 0;
   DWORD size = 0;
   std::vector<BYTE> signature;
   ALG_ID alg = CALG_SHA_256;
   HCRYPTPROV hProv = 0;
   LPTSTR szCSPName = NULL;
   SCARDCONTEXT hContext = NULL;
   LONG result = 0;
   DWORD readerLen = SCARD_AUTOALLOCATE;
   LPTSTR readerBuf = NULL;
   bool signResult = false;
   const size_t bufferLen = MAX_PATH + 11;
   WCHAR defaultContainer[bufferLen];
   DWORD  dwAutoAllocate = SCARD_AUTOALLOCATE;
   LPTSTR szCardName = NULL;
   Json::Value healthInfo = Json::objectValue;
   Json::Value scInfo = Json::objectValue;


   LOG("Check smart card...");

   result = SCardEstablishContext(SCARD_SCOPE_USER,
      NULL,
      NULL,
      &hContext);
   if (result != SCARD_S_SUCCESS) {
      LOG_ERROR("Fail to establish context.");
      return false;
   }

   std::vector<SCARD_READERSTATE> rgscState =
      GetReaderInfo(hContext, &readerBuf);
   if (rgscState.size() < 1) {
      LOG_ERROR("Fail to get reader. \n");
      healthInfo["HealthStatus"] = "None";
      healthInfo["Smart Card Reader"] = "None";
      healthInfo["Health Description"] = "No attached smart card reader.";;
      goto CLEAN_UP;
   }

   LOG("List smart cards...");
   for (int i = 0; i < rgscState.size(); i++) {
      LOG("Reader Name: %S", rgscState[i].szReader);
   }
   
   healthInfo["Smart Card Deployment"] = "OK";

   char* readerName = NULL;
   WideCharToMultiByte(rgscState[0].szReader, &readerName);
   scInfo["Reader Name"] = readerName;
   healthInfo["Smart Card Reader"] = "OK";
   free(readerName);
   // Only select first card from reader for testing
   result = SCardListCards(hContext,
      rgscState[0].rgbAtr,
      NULL,
      0,
      (LPTSTR)&szCardName,
      &dwAutoAllocate);
   if (result != SCARD_S_SUCCESS) {
      LOG_ERROR("Unable to list cards in reader.");
      healthInfo["HealthStatus"] = "None";
      healthInfo["Smart Card"] = "None";
      healthInfo["Health Description"] = "No attached smart card.";
      goto CLEAN_UP;
   }
   else {
      LOG("Card Name: %S", szCardName);
      char* cardName = NULL;
      WideCharToMultiByte(szCardName, &cardName);
      scInfo["Card Name"] = cardName;
      healthInfo["Smart Card"] = "OK";
      free(cardName);
   }

   /*
    * szCardName could be a multi-string, but it doesn't matter because all
    * we need the card name for is to retrieve the CSP and we can assume that
    * each card in a reader uses the same CSP.
    */
   dwAutoAllocate = SCARD_AUTOALLOCATE;
   LOG("SCardGetCardTypeProviderName...");
   result = SCardGetCardTypeProviderName(hContext,
      szCardName,
      SCARD_PROVIDER_CSP,
      (LPTSTR)&szCSPName,
      &dwAutoAllocate);
   if (result != SCARD_S_SUCCESS) {
      healthInfo["HealthStatus"] = "Error";
      healthInfo["Credential Provider"] = "Error";
      healthInfo["Health Description"] = "Unable to retrieve Credential Provider for the smart card. Please check whether the smart card driver is installed correctly.";
      LOG_ERROR("Unable to retrieve CSP for smart card %S", szCardName);
      goto CLEAN_UP;
   }
   else {
      LOG("CSP name: %S", szCSPName);
      char* cspName = NULL;
      WideCharToMultiByte(szCSPName, &cspName);
      scInfo["Credential Provider"] = cspName;
      healthInfo["Credential Provider"] = "OK";
      free(cspName);
   }

   // zero the buffer out first
   memset(defaultContainer, 0, sizeof(wchar_t) * bufferLen);
   // only write up to bufferLen - 1, i.e. ensure the last character is left zero
   _snwprintf(defaultContainer,
      bufferLen - 1,
      L"\\\\.\\%s\\",
      rgscState[0].szReader);
   LOG("Container: %S\n", defaultContainer);
   char* container = NULL;
   WideCharToMultiByte(defaultContainer, &container);
   scInfo["Container"] = container;
   free(container);

   LOG("Acquire card context...");
   if (!CryptAcquireContext(&hProv, defaultContainer, szCSPName, PROV_RSA_FULL,
      CRYPT_SILENT | CRYPT_MACHINE_KEYSET)) {
      LOG_ERROR("Fail to acquire context.");
      healthInfo["HealthStatus"] = "Error";
      healthInfo["Acquire Card Context"] = "Error";
      healthInfo["Health Description"] = "Fail to acquire card context.";

      goto CLEAN_UP;
   }

   if (!CryptSetProvParam(hProv, PP_KEYEXCHANGE_PIN, LPBYTE(pin.c_str()), 0)) {
      LOG_ERROR("Fail to set PIN."
         "Please check the PIN and smart card is inserted.");
      healthInfo["HealthStatus"] = "Error";
      healthInfo["Set Smart Card PIN"] = "Error";
      healthInfo["Health Description"] = "The Smart card pin checks failure.";

      goto CLEAN_UP;
   }

   HCRYPTKEY  privateKey;
   if (CryptGetUserKey(hProv, AT_KEYEXCHANGE, &privateKey)) {
      LOG("Get private key successfully.\n");
      healthInfo["Smart Card Verification"] = "OK";
      healthInfo["HealthStatus"] = "OK";
   }
   else {
      LOG_ERROR("Fail to get user key.");
      healthInfo["HealthStatus"] = "Error";
      healthInfo["Smart Card Verification"] = "Error";
      healthInfo["Health Description"] = "Fail to retrieve certificate. Check the smart card driver is installed correctly.";

      goto CLEAN_UP;
   }

CLEAN_UP:
   root["AgentHealthChecking"] = healthInfo;
   root["AgentSmartCardStatus"] = scInfo;
   if (readerBuf) {
      SCardFreeMemory(hContext, readerBuf);
      readerBuf = NULL;
   }
   if (szCSPName) {
      SCardFreeMemory(hContext, szCSPName);
      szCSPName = NULL;
   }
   if (hProv) {
      CryptReleaseContext(hProv, 0);
      hProv = NULL;
   }
   SCardReleaseContext(hContext);

   return signResult;
}
