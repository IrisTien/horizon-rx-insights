/* ****************************************************************************
 * Copyright 2019 VMware, Inc.  All rights reserved. -- VMware Confidential
 * ****************************************************************************/

 /*
  * scTestApp.cpp --
  *
  *    Encapsulates the 'main' function for Smart Card API Testing tool
  */


#include "logUtils.h"
#include "scTestUtils.h"
#include <stdio.h>
#include <windows.h>
#include <json/json.h>
#include <fstream>
#include <iostream>

using namespace std;


#define DEFAULT_PIN "123456"
#define RESULT_SUCCESS 0
#define RESULT_FAILURE -1

/*
 *----------------------------------------------------------------------
 *
 * TestAll --
 *
 *     Run all smart card test cases with smart card's PIN
 *
 * Results:
 *     true: Run successfully.
 *     false: Run failure
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

bool
TestAll(const char *pin) // IN
{
   bool result = true;
   LOG("============Import Certificates To MY Store============");
   result = ImportCertificatesToMYStore() && result;
   LOG("============Display All Certificates In MY Store============");
   result = DisplayAllCertificatesInMyStore() && result;
   LOG("=============================================================");
   LOG("============Retrieve smart card keys============");
   result = RetrieveCertificatesFromSmartCard() && result;
   LOG("=============================================================");
   LOG("============Sign test data with smart card's private key=======");
   result = SignWithSmartCard(pin) && result;
   LOG("============Crypt and decrypt test data with smart card========");
   result = EncryptAndDecryptWithSmartCard(pin) && result;
   LOG("=============================================================");
   LOG("===================Display smart card's ATR====================");
   std::string atr = GetCardATR();
   result = (atr.length() > 1) && result;
   if (result) {
      LOG("Run all tests successfully.");
   } else {
      LOG("Run all tests failure.");
   }
   return result;
}

/*
 *----------------------------------------------------------------------
 *
 * RunAllTestsThreadFun --
 *
 *     The thread function to run all tests
 *
 * Results:
 *     0: Run successfully
 *     -1: Run failed
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

DWORD
RunAllTestsThreadFun(LPVOID param) // IN
{
   const char *pin = (char *)param;
   return TestAll(pin) ? RESULT_SUCCESS : RESULT_FAILURE;
}


/*
 *----------------------------------------------------------------------
 *
 * ThreadFunRetrieveData --
 *
 *     The thread function to run below tests:
 *       Display all certificates in MY store
 *       Display all certificates in MY store
 *       Get card's ATR
 *
 * Results:
 *     0: Run successfully
 *     -1: Run failed
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

DWORD WINAPI
ThreadFunRetrieveData(LPVOID param) // IN
{
   const char *pin = (char *)param;
   bool result = true;
   LOG("===================Display smart card's ATR====================");
   std::string atr = GetCardATR();
   result = (atr.length() > 1) && result;

   LOG("============Import Certificates to MY Store============");
   result = ImportCertificatesToMYStore() && result;

   LOG("Test Result: %s", result ? "true" : "false");
   return result ? RESULT_SUCCESS : RESULT_FAILURE;
}


/*
 *----------------------------------------------------------------------
 *
 * ThreadFunSignEncryptData --
 *
 *     The thread function to run below tests:
 *       Sign data with Smart Card's certificate
 *       Get card's ATR
 *
 * Results:
 *     0: Run successfully
 *     -1: Run failed
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

DWORD WINAPI
ThreadFunSignEncryptData(LPVOID param) // IN
{
   const char *pin = (char *)param;
   bool result = true;
   LOG("============Sign test data with smart card's private key=======");
   result = SignWithSmartCard(pin) && result;
   LOG("============Crypt and decrypt test data with smart card========");
   result = EncryptAndDecryptWithSmartCard(pin) && result;

   LOG("Test Result: %s", result ? "true" : "false");
   return result ? RESULT_SUCCESS : RESULT_FAILURE;
}


/*
 *----------------------------------------------------------------------
 *
 * ConcurrentRunAllTests --
 *
 *     Launch multiple thread to concurrently run all tests
 *
 * Results:
 *     true: Run successful
 *     false: Otherwise
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

bool
ConcurrentRunAllTests(int threadCount, // IN
                      const char *pin) // IN
{
   DWORD *dwThreadIdArray = new DWORD[threadCount];
   HANDLE *hThreadArray = new HANDLE[threadCount];
   bool result = true;
   DWORD dwExit = -1;

   LOG("Concurrently run all tests...");
   // Disable console logging to avoid too much logging in console
   EnableConsoleLog(false);
   for (int i = 0; i < threadCount; i++) {
      LOG("Start thread %d...", i+1);
      LPTHREAD_START_ROUTINE threadFun;
      // Only first thread to perform sign/decrypt data
      if (i == 0) {
         threadFun = ThreadFunSignEncryptData;
      } else {
          threadFun = ThreadFunRetrieveData;
      }
      hThreadArray[i] = CreateThread(NULL,                   // default security attributes
                                     0,                      // use default stack size
                                     threadFun,              // thread function name
                                     (LPVOID)pin,            // argument to thread function
                                     0,                      // use default creation flags
                                     &dwThreadIdArray[i]);   // returns the thread identifier
      if (hThreadArray[i] == NULL) {
         LOG_ERROR("Fail to create thread.");
         result = false;
         goto CLEAN_UP;
      }
    }

   // Wait until all threads have terminated.
   WaitForMultipleObjects(threadCount, hThreadArray, TRUE, INFINITE);

   for (int i=0; i< threadCount; i++) {
      if (GetExitCodeThread(hThreadArray[i], &dwExit)) {
         LOG("Thread %d exit code: %d", i, dwExit);
         result = ((dwExit == 0) && result);
      }
   }

CLEAN_UP:
   // Close all thread handles and free memory allocations.
   for(int i=0; i< threadCount; i++) {
      if (hThreadArray[i]) {
         CloseHandle(hThreadArray[i]);
      }
   }

   EnableConsoleLog(true);
   LOG("Concurrent Test Result: %s", result ? "true" : "false");
   free(dwThreadIdArray);
   free(hThreadArray);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * DisplayHelp --
 *
 *     Display help content for all supported parameters
 *
 * Results:
 *     None.
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

void
DisplayHelp() {
   LOG("Smart Card API Testing Tool\n");
   LOG("Usage:\n");
   LOG("-pin xxxxxx: Set smart card's PIN");
   LOG("-ListCerts: Display all certificates in MY stroe.\n\n");
   LOG("-ImportCerts: Import certificates to MY stroe.\n\n");
   LOG("-GetKeys: Retrieve smart card key in Smart Card Key Store.\n\n");
   LOG("-Sign: Sign test data with smart card's private key.\n\n");
   LOG("-EncryptDecrypt: Encrypt and decrypt test data with smart card's private key.\n\n");
   LOG("-ConcurrentTest [n]: Concurrently run all tests. n is the concurrent thread count.\n\n");
   LOG("-GetATR: Get Smart Card's ATR.\n\n");
   LOG("-AllTests: Run all above testings.\n\n");
}



bool VerifySmartCardClientInfo(Json::Value& hRoot) {
   std::string filePath = "c:\\test_path\\devices.json";
   Json::Reader reader;
   std::ifstream ifs;
   bool bResult = false;
   Json::Value scInfo;
   Json::Value scHealthInfo;
   Json::Value root;
   bool healthy = true;

   LOG("Load the device info: %s\n", filePath.c_str());
   ifs.open(filePath.c_str(), std::ifstream::binary);
   if (!ifs.is_open()) {
      LOG_ERROR("Fail to open file: %s", filePath.c_str());
      return false;
   }

   if (!reader.parse(ifs, root)) {
      LOG_ERROR("Fail to parse the file %s \n", filePath.c_str());
      return bResult;
   }

   Json::Value dataTypes = root["SPSmartCardsDataType"];
   if (dataTypes.empty()) {
      LOG_ERROR("Cannot find SPSmartCardsDataType from %s", filePath.c_str());
      scHealthInfo["Health Description"] = "Fail to retrieve certificate. Check the smart card driver is installed correctly.";

      return bResult;
   }

   int scCount = 0;
   int cReader = 0;
   int cReaderDriver = 0;
   int cCard = 0;
   int cTokenCert = 0;
   int cCTKCert = 0;
   scHealthInfo["HealthStatus"] = "None";
   for (Json::Value data : dataTypes) {
      std::string name = data["_name"].asString();
      if (name.empty()) {
         continue;
      }

      if (name == "READERS") {
         int idx = 1;
         while (true) {
            std::string idxName = std::string("#0") + std::to_string(idx);
            if (!data[idxName.c_str()].empty()) {
               cReader++;
               std::string reader = data[idxName.c_str()].asString();
               scInfo["Readers"].append(reader.c_str());

               if (idx == 1) {
                  scHealthInfo["Smart Card Reader"] = "OK";
               }
               
            } else {
               break;
            } // endif
            idx++;
         }// endwhile
         if (cReader == 0) {
            scHealthInfo["Smart Card Reader"] = "Error";
            scHealthInfo["HealthStatus"] = "Error";
         }
      } else if (name == "READERS_DRIVERS") {
         int idx = 1;
         while (true) {
            std::string idxName = std::string("#0") + std::to_string(idx);
            if (!data[idxName.c_str()].empty()) {
               cReaderDriver++;
               std::string driver = data[idxName.c_str()].asString();
               scInfo["Reader Drivers"].append(driver.c_str());

               if (idx == 1) {
                  scHealthInfo["Smart Card Reader Drivers"] = "OK";
               }
            }
            else {
               break;
            } // endif
            idx++;
         }// endwhile
         if (cReaderDriver == 0)
         {
            scHealthInfo["Smart Card Reader Drivers"] = "Error";
            scHealthInfo["HealthStatus"] = "Error";
         }
      }
      else if (name == "TOKEN_DRIVERS") {
         int idx = 1;
         while (true) {
            std::string idxName = std::string("#0") + std::to_string(idx);
            if (!data[idxName.c_str()].empty()) {
               std::string driver = data[idxName.c_str()].asString();
               scInfo["Tokend Drivers"].append(driver.c_str());

               if (idx == 1) {
                  scHealthInfo["Smart Card Tokend Drivers"] = "OK";
               }
            }
            else {
               break;
            } // endif
            idx++;
         }// endwhile
      } else if (name == "SMARTCARDS_DRIVERS") {
         int idx = 1;
         while (true) {
            std::string idxName = std::string("#0") + std::to_string(idx);
            if (!data[idxName.c_str()].empty()) {
               std::string driver = data[idxName.c_str()].asString();
               scInfo["CTK Drivers"].append(driver.c_str());

               if (idx == 1) {
                  scHealthInfo["Smart Card CTK Drivers"] = "OK";
               }
            }
            else {
               break;
            } // endif
            idx++;
         }// endwhile
      } else if (name == "AVAIL_SMARTCARDS_KEYCHAIN") {
         Json::Value items = data["_items"];
         if (items.size() < 1) {
            continue;
         }

         for (Json::Value item : items) {
            std::string name = item["_name"].asString();
            scInfo["Smart Card Keychain"].append(name.c_str());
            cTokenCert++;
         }// end for
         if (items.size() > 0) {
            scHealthInfo["Smart Card Certification in Keychain"] = "OK";

         }
      } else if (name == "AVAIL_SMARTCARDS_TOKEN") {
         Json::Value items = data["_items"];
         if (items.size() < 1) {
            continue;
         }

         for (Json::Value item : items) {
            std::string name = item["_name"].asString();
            scInfo["Smart Card Tokens"].append(name.c_str());
            cCTKCert++;
         }
         if (items.size() > 0) {
            scHealthInfo["Smart Card Tokens Checking"] = "OK";
         }
      }
   }

   if (cTokenCert > 0 || cCTKCert > 0) 
   {
      scHealthInfo["HealthStatus"] = "OK";
   }

   hRoot["ClientHealthChecking"] = scHealthInfo;
   hRoot["ClientSmartCardStatus"] = scInfo;
   /*
    "SPSmartCardsDataType" : [
    {
      "_name" : "READERS",
      "#01" : "Yubico YubiKey OTP+FIDO+CCID (ATR:{length = 23, bytes = 0x3bfd1300008131fe158073c021c057597562694b657940})"
    },
    {
      "_name" : "READERS_DRIVERS",
      "#01" : "org.debian.alioth.pcsclite.smartcardccid:1.4.32 (/usr/libexec/SmartCardServices/drivers/ifd-ccid.bundle)",
      "#02" : "com.SafeNet.eTokenIfdh:9.0.0.0 (/Library/Frameworks/eToken.framework/Versions/A/aks-ifdh.bundle)",
      "#03" : "(null):(null) (/Library/Frameworks/eToken.framework/Versions/A/ikey-ifdh.bundle)"
    },
    {
      "_name" : "TOKEN_DRIVERS",
      "#01" : "com.Safenet.eTokend:9.0 (/Library/Frameworks/eToken.framework/Versions/A/eTokend.tokend)"
    },
    {
      "_name" : "SMARTCARDS_DRIVERS",
      "#01" : "com.apple.CryptoTokenKit.pivtoken:1.0 (/System/Library/Frameworks/CryptoTokenKit.framework/PlugIns/pivtoken.appex)"
    },
    {
      "_items" : [
        {
          "_name" : "com.apple.setoken:aks"
        },
        {
          "_name" : "com.apple.pivtoken:2B00431656F53A11258B545E4B23B660",
          "#01" : "Kind: private RSA 2048-bit, Certificate: {length = 20, bytes = 0x76a11b490e39ffe396716b483ceb0e36a0813ab1}, Usage: Sign \nValid from: 2018-03-13 04:00:49 +0000 to: 2020-05-19 03:59:59 +0000, SSL trust: NO, X509 trust: YES \n\n-----BEGIN CERTIFICATE-----\nMIIE4TCCA8mgAwIBAgIDAJtiMA0GCSqGSIb3DQEBCwUAMGAxCzAJBgNVBAYTAlVTMRgwFgYDVQQKEw9VLlMuIEdvdmVybm1lbnQxDDAKBgNVBAsTA0RvRDEMMAoGA1UECxMDUEtJMRswGQYDVQQDExJET0QgUEIgSUQgU1cgQ0EtNDYwHhcNMTgwMzEzMDQwMDQ5WhcNMjAwNTE5MDM1OTU5WjB3MQswCQYDVQQGEwJVUzEYMBYGA1UEChMPVS5TLiBHb3Zlcm5tZW50MQwwCgYDVQQLEwNEb0QxDDAKBgNVBAsTA1BLSTENMAsGA1UECxMERFBNTzEjMCEGA1UEAxMaV2lsbGlhbS5SLk9ydGl6LjY2MDkxMDM2MzcwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCm/9HCa0YkqKLOgprm0IyiXA/FSJU90qHHGvYtRQvmJmQgYy9Ty0wV+AmNaWjFHr4cdLYDlmk0GGUmqORVKSUuAHXu2y1Vhw/mCaN3yx2wUluTnwe+ItrYECWVRqBYeNq8pZqOt5BfL+PFPyT9es9oQ/jPhWKafeV2dehFEs9b6wYZAQJF8n21Yyttq8gYdcTY1mMRpgLSAfAHUDyZv/JRgnb1jyfGNhqGpoBXoPEJk07PwCno0rl7mplcKpjpgh8NKnogkoJhSwwaPiIMP7f5v3ViVy6saSt+acHK7Xs/hRLft9HT6WvZE3iDWl8u8RJ7E1xZnasccd/AjqWfYTe5AgMBAAGjggGLMIIBhzAfBgNVHSMEGDAWgBSOwI1nQfPGmXCddi8L/R3/Hd7vTDAdBgNVHQ4EFgQUdqEbSQ45/+OWcWtIPOsONqCBOrEwDgYDVR0PAQH/BAQDAgeAMB8GA1UdJQQYMBYGCCsGAQUFBwMCBgorBgEEAYI3FAICMBYGA1UdIAQPMA0wCwYJYIZIAWUCAQsnMCkGA1UdEQQiMCCgHgYKKwYBBAGCNxQCA6AQDA42NjA5MTAzNjM3QG1pbDCBhQYIKwYBBQUHAQEEeTB3MEUGCCsGAQUFBzAChjlodHRwOi8vcGJjcmwucmVkaG91bmRzb2Z0d2FyZS5uZXQvc2lnbi9ET0RQQklEU1dDQV80Ni5jZXIwLgYIKwYBBQUHMAGGImh0dHA6Ly9wYm9jc3AucmVkaG91bmRzb2Z0d2FyZS5uZXQwSQYDVR0fBEIwQDA+oDygOoY4aHR0cDovL3BiY3JsLnJlZGhvdW5kc29mdHdhcmUubmV0L2NybC9ET0RQQklEU1dDQV80Ni5jcmwwDQYJKoZIhvcNAQELBQADggEBAFN/n0QMfoomeuGVFiGT7etSSWA9cPk3GwznQgCSAXJuftVAgZ/teIPftuI9T5Rtvlq+yKDhxcbPrF3mPfZUSComs8oyWuUucc/mofwNtuc3nq2vBWlUfh3yL0zbnJxpgrpGM/G66b67EIZ8i6sIimwtB0YghU1UGaTn2DCvkiFMVecrBOaoxv8js4Ct18PC93M/hpObuojT/x00/T1zYRGE1nuWYJHOamL3wC8cDy0u6lLx77xHWu0qshPu3N/Xp/xv259lkdYpSBVXbWpC1mzS6jfcIPePGvvfv9VGnTaecCHDlwD75x7jftXha4n3PphG23EfJIHKAJMxae1wnPI=\n-----END CERTIFICATE-----\n"
        }
      ],
      "_name" : "AVAIL_SMARTCARDS_KEYCHAIN"
    },
    {
      "_items" : [
        {
          "_name" : "com.apple.setoken:aks"
        },
        {
          "_name" : "com.apple.pivtoken:2B00431656F53A11258B545E4B23B660",
          "#01" : "Kind: private RSA 2048-bit, Certificate: {length = 20, bytes = 0x76a11b490e39ffe396716b483ceb0e36a0813ab1}, Usage: Sign \nValid from: 2018-03-13 04:00:49 +0000 to: 2020-05-19 03:59:59 +0000, SSL trust: NO, X509 trust: YES \n\n-----BEGIN CERTIFICATE-----\nMIIE4TCCA8mgAwIBAgIDAJtiMA0GCSqGSIb3DQEBCwUAMGAxCzAJBgNVBAYTAlVTMRgwFgYDVQQKEw9VLlMuIEdvdmVybm1lbnQxDDAKBgNVBAsTA0RvRDEMMAoGA1UECxMDUEtJMRswGQYDVQQDExJET0QgUEIgSUQgU1cgQ0EtNDYwHhcNMTgwMzEzMDQwMDQ5WhcNMjAwNTE5MDM1OTU5WjB3MQswCQYDVQQGEwJVUzEYMBYGA1UEChMPVS5TLiBHb3Zlcm5tZW50MQwwCgYDVQQLEwNEb0QxDDAKBgNVBAsTA1BLSTENMAsGA1UECxMERFBNTzEjMCEGA1UEAxMaV2lsbGlhbS5SLk9ydGl6LjY2MDkxMDM2MzcwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCm/9HCa0YkqKLOgprm0IyiXA/FSJU90qHHGvYtRQvmJmQgYy9Ty0wV+AmNaWjFHr4cdLYDlmk0GGUmqORVKSUuAHXu2y1Vhw/mCaN3yx2wUluTnwe+ItrYECWVRqBYeNq8pZqOt5BfL+PFPyT9es9oQ/jPhWKafeV2dehFEs9b6wYZAQJF8n21Yyttq8gYdcTY1mMRpgLSAfAHUDyZv/JRgnb1jyfGNhqGpoBXoPEJk07PwCno0rl7mplcKpjpgh8NKnogkoJhSwwaPiIMP7f5v3ViVy6saSt+acHK7Xs/hRLft9HT6WvZE3iDWl8u8RJ7E1xZnasccd/AjqWfYTe5AgMBAAGjggGLMIIBhzAfBgNVHSMEGDAWgBSOwI1nQfPGmXCddi8L/R3/Hd7vTDAdBgNVHQ4EFgQUdqEbSQ45/+OWcWtIPOsONqCBOrEwDgYDVR0PAQH/BAQDAgeAMB8GA1UdJQQYMBYGCCsGAQUFBwMCBgorBgEEAYI3FAICMBYGA1UdIAQPMA0wCwYJYIZIAWUCAQsnMCkGA1UdEQQiMCCgHgYKKwYBBAGCNxQCA6AQDA42NjA5MTAzNjM3QG1pbDCBhQYIKwYBBQUHAQEEeTB3MEUGCCsGAQUFBzAChjlodHRwOi8vcGJjcmwucmVkaG91bmRzb2Z0d2FyZS5uZXQvc2lnbi9ET0RQQklEU1dDQV80Ni5jZXIwLgYIKwYBBQUHMAGGImh0dHA6Ly9wYm9jc3AucmVkaG91bmRzb2Z0d2FyZS5uZXQwSQYDVR0fBEIwQDA+oDygOoY4aHR0cDovL3BiY3JsLnJlZGhvdW5kc29mdHdhcmUubmV0L2NybC9ET0RQQklEU1dDQV80Ni5jcmwwDQYJKoZIhvcNAQELBQADggEBAFN/n0QMfoomeuGVFiGT7etSSWA9cPk3GwznQgCSAXJuftVAgZ/teIPftuI9T5Rtvlq+yKDhxcbPrF3mPfZUSComs8oyWuUucc/mofwNtuc3nq2vBWlUfh3yL0zbnJxpgrpGM/G66b67EIZ8i6sIimwtB0YghU1UGaTn2DCvkiFMVecrBOaoxv8js4Ct18PC93M/hpObuojT/x00/T1zYRGE1nuWYJHOamL3wC8cDy0u6lLx77xHWu0qshPu3N/Xp/xv259lkdYpSBVXbWpC1mzS6jfcIPePGvvfv9VGnTaecCHDlwD75x7jftXha4n3PphG23EfJIHKAJMxae1wnPI=\n-----END CERTIFICATE-----\n"
        }
      ],
      "_name" : "AVAIL_SMARTCARDS_TOKEN"
    }
  ]*/
/*
   Json::Value root;
   Json::Reader reader;
   std::ifstream ifs;
   bool bResult = false;

   LOG_INFO("Load policy setting from config file: %s\n", filePath.c_str());
   ifs.open(filePath.c_str(), std::ifstream::binary);
   if (!ifs.is_open()) {
      LOG_ERROR("Fail to open config file: %s", filePath.c_str());
      return false;
   }

   if (!reader.parse(ifs, root)) {
      LOG_ERROR("Fail to parse the config file %s \n", filePath.c_str());
      return bResult;
   }*/
}


/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *    The main function for the application
 *
 * Results:
 *     0: Run successfully.
 *     Others: Otherwise
 *
 * Side Effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

int
main(int argc, char *argv[])
{
   char pin[100] = DEFAULT_PIN;
   bool result = false;
   int exitCode = -1;

   // Initialize the log file
   InitLog();

   for (int i = 0; i < argc; i++) {
      if (strcmp(argv[i], "-pin") == 0) {
         i++;
         size_t length = strlen(argv[i]);
         memcpy(pin, argv[i], length);
         pin[length] = '\0';
      } else if(strcmp(argv[i], "-ListCerts") == 0) {
         result = DisplayAllCertificatesInMyStore();
      } else if(strcmp(argv[i], "-ImportCerts") == 0) {
         result = ImportCertificatesToMYStore();
      } else if (strcmp(argv[i], "-GetKeys") == 0) {
         result = RetrieveCertificatesFromSmartCard();
      } else if (strcmp(argv[i], "-Sign") == 0) {
         Json::Value root = Json::objectValue;
         result = SignWithSmartCard(pin);
      }  else if (strcmp(argv[i], "-EncryptDecrypt") == 0) {
         result = EncryptAndDecryptWithSmartCard(pin);
      } else if (strcmp(argv[i], "-ConcurrentTest") == 0) {
         i++;
         int threadCount = 1;
         const int MAX_THREAD = 10;
         if (i < argc) {
            threadCount = atoi(argv[i]);
            LOG("Thread count: %d", threadCount);
            if (threadCount > MAX_THREAD || threadCount < 1) {
               LOG("The thread count is invalid. The valid thread is from 1 to 10.");
               goto END;
            }
         }
         result = ConcurrentRunAllTests(threadCount, pin);
      } else if (strcmp(argv[i], "-GetATR") == 0) {
         result = (GetCardATR().length() > 0);
      } else if (strcmp(argv[i], "-AllTests") == 0) {
         result = TestAll(pin);
      } else if (strcmp(argv[i], "-h") == 0) {
         DisplayHelp();
      } else {
         Json::Value root = Json::objectValue;
         result = VerifySmartCard(pin, root);
         Json::FastWriter fastWriter;


         result = result & VerifySmartCardClientInfo(root);
         root["Feature Name"] = "Smart Card";
         bool healthy = false;
         std::cout << root["ClientHealthChecking"]["HealthStatus"].asString().c_str() << endl;
         if (root["AgentHealthChecking"]["HealthStatus"].asString() == "Error" || root["ClientHealthChecking"]["HealthStatus"].asString() == "Error"
            ) {
            root["Overall Health Status"] = "Unhealthy";
         }
         else if (root["AgentHealthChecking"]["HealthStatus"].asString() == "None" || root["ClientHealthChecking"]["HealthStatus"].asString() == "None"
            ) {
            root["Overall Health Status"] = "None";
         }
         else {
            root["Overall Health Status"] = "Healthy";
         }
         Json::Value hcRoot = Json::objectValue;
         hcRoot["HealthStatus"] = root;
         std::string scResult = fastWriter.write(root);
         ofstream sHealth;
         sHealth.open("C:\\test_path\\SmartCardHealthStatus.json");
         sHealth << scResult.c_str();
         sHealth.close();
         LOG("%s", scResult.c_str());
      }
      
   }

END:
   LOG("Test Result File: %s", GetLogFileName());
   exitCode = result ? RESULT_SUCCESS : RESULT_FAILURE;
   LOG("Exit code: %d", exitCode);
   return exitCode;
}
