import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { of } from 'rxjs';
import { API_HELPDESK } from '^utilities/constants/API.constant';

const MOCK_RX_DATA = {
  session_info: {
    ID: '6',
    ClientType: 'Windows',
    Protocol: 'BLAST',
    ClientMachineName: 'zhikai-PC',
    ClientLaunchId: 'Win10-2004-VDI-1',
    ClientId: 'AgAAAERFTExLABAygDjKwE9EMDI=',
    ClientVersion: '5.0.0-12606690',
    Domain: 'viewbj',
    FarmId: 'win10-2004-vdi-1',
    User: 'administrator',
    BrokerDns: 'WIN-9HU6H2J52PN.viewbj.com'
  },
  health_info: {
    SmartCard: {
      SmartCardHealth: {
        AgentHealthChecking: {
          'Credential Provider': 'OK',
          HealthStatus: 'OK',
          'Smart Card': 'OK',
          'Smart Card Deployment': 'OK',
          'Smart Card Reader': 'OK',
          'Smart Card Verification': 'OK'
        },
        AgentSmartCardStatus: {
          'Card Name': 'ActivID ActivClient (Gemalto TOP DL 144K FIPS)',
          Container: '\\\\.\\ActivIdentity USB Reader V3\\',
          'Credential Provider': 'Microsoft Base Smart Card Crypto Provider',
          'Reader Name': 'ActivIdentity USB Reader V3'
        },
        ClientHealthChecking: {
          HealthStatus: 'OK',
          'Smart Card CTK Drivers': 'OK',
          'Smart Card Certification in Keychain': 'OK',
          'Smart Card Reader': 'OK',
          'Smart Card Reader Drivers': 'OK',
          'Smart Card Tokend Drivers': 'OK',
          'Smart Card Tokens Checking': 'OK'
        },
        ClientSmartCardStatus: {
          'CTK Drivers':
            'com.apple.CryptoTokenKit.pivtoken:1.0 (/System/Library/Frameworks/CryptoTokenKit.framework/PlugIns/pivtoken.appex)',
          'Reader Drivers': 'org.debian.alioth.pcsclite.smartcardccid:1.4.32 (/usr/libexec/SmartCardServices/drivers/ifd-ccid.bundle)',
          'com.SafeNet.eTokenIfdh:9.0.0.0 (/Library/Frameworks/eToken.framework/Versions/A/aks-ifdh.bundle)':
            '(null):(null) (/Library/Frameworks/eToken.framework/Versions/A/ikey-ifdh.bundle)',
          Readers: 'Yubico YubiKey OTP+FIDO+CCID (ATR:{length = 23, bytes = 0x3bfd1300008131fe158073c021c057597562694b657940})',
          'Smart Card Keychain': 'com.apple.setoken:aks',
          'com.apple.pivtoken:2B00431656F53A11258B545E4B23B660': 'Smart Card Tokens',
          'com.apple.setoken:aks': 'com.apple.pivtoken:2B00431656F53A11258B545E4B23B660',
          'Tokend Drivers': 'com.Safenet.eTokend:9.0 (/Library/Frameworks/eToken.framework/Versions/A/eTokend.tokend)'
        },
        'Feature Name': 'Smart Card',
        'Overall Health Status': 'Healthy'
      }
    },
    Clipboard: {
      ClipboardHealth: {
        HealthStatus: {
          'Feature Name': 'Clipboard',
          'Overall Health Status': 'Healthy',
          'Policy Setting': {
            ClipboardState: {
              PolicyName: 'Configure Clipboard Direction',
              'Registry HIVE': 'HKLM',
              Value: 'Enabled in both directions'
            },
            ClipboardSize: {
              PolicyName: 'Clipboard Memory Size On Server',
              'Registry HIVE': 'HKLM',
              Value: '2048 Kilobytes'
            }
          },
          'Service Status': {
            'VMwareViewClipboard.exe': 'Running'
          },
          'User Operations': {
            'Has Error': 'No'
          }
        }
      },
      ClipboardOperations: {
        'User Operations': {
          Description: 'Copy 116 bytes text data from Horizon Agent to Client',
          Level: 'Information'
        },
        Description: 'Copy 4M bytes image data from Horizon Client to Agent',
        Level: 'Error'
      }
    },
    Devices: {
      DeviceList: {
        SPPrintersDataType: {
          _name: 'Canon G3000 series'
        },
        SPSmartCardsDataType: {
          _name: 'READERS',
          '#01': 'Yubico YubiKey OTP+FIDO+CCID (ATR:{length = 23, bytes = 0x3bfd1300008131fe158073c021c057597562694b657940})'
        }
      }
    },
    URLRedirect: {
      URLRedirectHealth: {
        HealthStatus: {
          'Feature Name': 'URL Content Redirection',
          'Overall Health Status': 'Healthy',
          'Policy Setting': {
            URLRedirectionEnabled: {
              PolicyName: 'URL Redirection Enabled',
              'Registry HIVE': 'HKLM',
              Value: 'Enabled'
            },
            URLRedirectionIPRulesEnabled: {
              PolicyName: 'URL Redirection IP Rules Enabled',
              'Registry HIVE': 'HKLM',
              Value: 'Enabled'
            },
            URLRedirectionSide: {
              PolicyName: 'URL Redirection Side',
              'Registry HIVE': 'HKLM',
              Value: 'Agent'
            },
            URLRedirectionProtocolHttps: {
              PolicyName: 'URL Redirection Protocol https',
              'Registry HIVE': 'HKLM',
              Value: {
                brokerHostname: '',
                remoteItem: '',
                clientRules: 'vmware.com;.*.testdell.com;.*a{4}custom',
                agentRules: 'baidu.com;chinanihao.cc;.*train[^abc]'
              }
            }
          },
          'Service Status': {
            'vmware-url-protocol-launch-helper.exe': 'Running',
            'VMwareView-RdeServer.exe': 'Running'
          }
        }
      }
    }
  }
};

@Injectable({
  providedIn: 'root'
})
export class RxService {
  mockRxData = MOCK_RX_DATA;
  constructor(private http: HttpClient) {}

  settings = [
    {
      policyName: ''
    }
  ];

  getRXOverallData() {
    return of({
      nodes: [
        {
          id: 'daec2645-1844-4436-a5c7-7c5664f6eb18',
          name: 'ViewBJ-POD1',
          status: 'READY',
          tenantId: '1825b61d-d350-4820-8328-8ac60f031fa1',
          type: 'POD',
          connectionServers: [
            {
              serverUrl: 'https://RX-CS-2019-0.viewbj.com:443',
              serverName: 'RX-CS-2019-0',
              localConnectionServer: true,
              version: '8.1.0-17088772',
              type: 'Server'
            }
          ],
          pools: [
            {
              cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
              poolid:
                'RGVza3RvcC9OV1k1T0RJNU16RXRZV0ZsWkMwME16SmlMV0ZoWXpjdFpHVmxZelpsTWpjMlltRXcvZDJsdU1UQXRNakF3TkMxMlpHa3RNUQ-127d0c68-9d37-4eed-a060-e09cc3f25560',
              pool: 'Win10-2004-VDI-1',
              product: 'enzo',
              deployment: 'onprem',
              smartnodeid: '127d0c68-9d37-4eed-a060-e09cc3f25560',
              type: 'pool'
            },
            {
              cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
              poolid:
                'RGVza3RvcC9OV1k1T0RJNU16RXRZV0ZsWkMwME16SmlMV0ZoWXpjdFpHVmxZelpsTWpjMlltRXcvZDJsdU1UQXRNakF3TkMxMlpHa3RNZw-127d0c68-9d37-4eed-a060-e09cc3f25560',
              pool: 'Win10-2004-VDI-2',
              product: 'enzo',
              deployment: 'onprem',
              smartnodeid: '127d0c68-9d37-4eed-a060-e09cc3f25560',
              type: 'pool'
            }
          ]
        },
        {
          id: '127d0c68-9d37-4eed-a060-e09cc3f25560',
          name: 'ViewBJ-PoD2',
          status: 'READY',
          tenantId: '1825b61d-d350-4820-8328-8ac60f031fa1',
          type: 'POD',
          connectionServers: [
            {
              serverUrl: 'https://10.117.44.21:443',
              serverName: 'WIN-9HU6H2J52PN',
              localConnectionServer: true,
              version: '8.1.0-17094412',
              type: 'Server'
            }
          ],
          pools: [
            // {
            //   type: 'pool',
            //   cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
            //   poolid:
            //     'RGVza3RvcC9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vYTJWeWJtVnNaR1ZpZFdkaFoyVnVkQQ-daec2645-1844-4436-a5c7-7c5664f6eb18',
            //   pool: 'KernelDebugAgent',
            //   product: 'enzo',
            //   deployment: 'onprem',
            //   smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            // },
            // {
            //   type: 'pool',
            //   cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
            //   poolid:
            //     'RGVza3RvcC9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vYzIxaGNuUmpZWEprWDNkcGJqRXdlRFkwTFRFNU1Ea3RZMmhoY21semJXRjBhR2xqY3c-daec2645-1844-4436-a5c7-7c5664f6eb18',
            //   pool: 'SmartCard_Win10x64-1909-Charismathics',
            //   product: 'enzo',
            //   deployment: 'onprem',
            //   smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            // },
            // {
            //   type: 'pool',
            //   cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
            //   poolid:
            //     'RGVza3RvcC9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vYzIxaGNuUmpZWEprWDNkcGJqRXdlRFkwTFRFNU1Ea3RZV04wYVhabFkyeHBaVzUw-daec2645-1844-4436-a5c7-7c5664f6eb18',
            //   pool: 'SmartCard_Win10x64-1909-ActiveClient',
            //   product: 'enzo',
            //   deployment: 'onprem',
            //   smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            // },
            // {
            //   type: 'pool',
            //   cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
            //   poolid:
            //     'RGVza3RvcC9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vYzIxaGNuUmpZWEprWDNkcGJqRXdlRFkwTFRFNU1Ea3RjMkZtWlc1bGRB-daec2645-1844-4436-a5c7-7c5664f6eb18',
            //   pool: 'SmartCard_Win10x64-1909-Safenet',
            //   product: 'enzo',
            //   deployment: 'onprem',
            //   smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            // },
            // {
            //   type: 'pool',
            //   cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
            //   poolid:
            //     'RGVza3RvcC9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vYzIxaGNuUmpZWEprWDNkcGJqRXdlRFkwWHpJd01EUXRZMmhoY21semJXRjBhR2xqY3c-daec2645-1844-4436-a5c7-7c5664f6eb18',
            //   pool: 'SmartCard_Win10x64_2004-Charismathics',
            //   product: 'enzo',
            //   deployment: 'onprem',
            //   smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            // },
            // {
            //   type: 'pool',
            //   cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
            //   poolid:
            //     'RGVza3RvcC9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vYzIxaGNuUmpZWEprWDNkcGJqRXdlRFkwWHpJd01EUXRZV04wYVhaamJHbGxiblIw-daec2645-1844-4436-a5c7-7c5664f6eb18',
            //   pool: 'SmartCard_Win10x64_2004-ActivClientt',
            //   product: 'enzo',
            //   deployment: 'onprem',
            //   smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            // },
            // {
            //   type: 'pool',
            //   cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
            //   poolid:
            //     'RGVza3RvcC9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vYzIxaGNuUmpZWEprWDNkcGJqRXdlRFkwWHpJd01EUXRjMkZtWlc1bGRB-daec2645-1844-4436-a5c7-7c5664f6eb18',
            //   pool: 'SmartCard_Win10x64_2004-Safenet',
            //   product: 'enzo',
            //   deployment: 'onprem',
            //   smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            // },
            {
              type: 'pool',
              cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
              poolid:
                'RGVza3RvcC9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vZDJsdU1UQmZNVGt3T1E-daec2645-1844-4436-a5c7-7c5664f6eb18',
              pool: 'Win10_1909',
              product: 'enzo',
              deployment: 'onprem',
              smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            },
            {
              type: 'pool',
              cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
              poolid:
                'RGVza3RvcC9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vZDJsdU1UQmZNVGt3T1Y4eQ-daec2645-1844-4436-a5c7-7c5664f6eb18',
              pool: 'Win10_1909_2',
              product: 'enzo',
              deployment: 'onprem',
              smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            },
            {
              type: 'pool',
              cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
              poolid:
                'RGVza3RvcC9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vZDJsdU1UQmZNakF3TkY4eA-daec2645-1844-4436-a5c7-7c5664f6eb18',
              pool: 'Win10_2004_1',
              product: 'enzo',
              deployment: 'onprem',
              smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            },
            {
              type: 'pool',
              cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
              poolid:
                'RGVza3RvcC9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vZDJsdU1UQmZNakF3TkY4eQ-daec2645-1844-4436-a5c7-7c5664f6eb18',
              pool: 'Win10_2004_2',
              product: 'enzo',
              deployment: 'onprem',
              smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            },
            {
              type: 'pool',
              cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
              poolid:
                'RmFybS9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vWm1GeWJWOHlNREU1-daec2645-1844-4436-a5c7-7c5664f6eb18',
              pool: 'Farm_2019',
              product: 'enzo',
              deployment: 'onprem',
              smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            },
            // {
            //   type: 'pool',
            //   cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
            //   poolid:
            //     'RmFybS9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vYzIxaGNuUmpZWEprTFhkcGJqSXdNVGt0WTJoaGNtbHpiV0YwYUdsamN3-daec2645-1844-4436-a5c7-7c5664f6eb18',
            //   pool: 'SmartCard-Win2019-Charismathics',
            //   product: 'enzo',
            //   deployment: 'onprem',
            //   smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            // },
            // {
            //   type: 'pool',
            //   cmstenantid: 'yvusmckdz0mgeotfvjwvnhyh_cq',
            //   poolid:
            //     'RmFybS9aR1F3TTJaaFpqVXRNMlF4TXkwME9HWmtMV0l6WkRrdE1HTmxNR1V3TWpKbU5XVm0vYzIxaGNuUmpZWEprTFhkcGJqSXdNVGt0WVdOMGFYWmpiR2xsYm5SellXWmxibVYw-daec2645-1844-4436-a5c7-7c5664f6eb18',
            //   pool: 'SmartCard-Win2019-ActivClientSafenet',
            //   product: 'enzo',
            //   deployment: 'onprem',
            //   smartnodeid: 'daec2645-1844-4436-a5c7-7c5664f6eb18'
            // }
          ]
        }
      ]
    });
  }

  getRXData() {
    return of(MOCK_RX_DATA);
  }

  getRXDataFromHelpdesk(podId, sessionId) {
    return this.http.post(API_HELPDESK.GET_REMOTE_ASSIST(podId), {
      id: sessionId
    });
  }

  //`/side=Client&agentRules=vmware.com;www.baidu.com;.*.test.com&clientRules=.baidu.com;nihao.com;\.vmware\.test\.com&testUrl=http://www.baidu.com:6000`
  //`/policyverify/?side=Client&agentRules=vmware.com;www.baidu.com;.*.test.com&clientRules=.baidu.com;nihao.com;\.vmware\.test\.com&testUrl=http://www.baidu.com:6000`
  verifyPolicy(side, agentRules, clientRules, testUrl) {
    // const url = 'https://10.117.41.153:6600';
    const url = '/policyverify';
    return this.http.get(`${url}/?side=${side}&agentRules=${agentRules}&clientRules=${clientRules}&testUrl=${testUrl}`, {
      headers: {
        'Content-Type': 'application/json'
      }
    });
  }
}
