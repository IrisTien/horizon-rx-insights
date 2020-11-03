import { Component, Input, OnInit, QueryList, ViewChild, ViewChildren } from '@angular/core';
import { L10nService } from '@vmw/ngx-vip';
import { BehaviorSubject } from 'rxjs-compat';
import { each, isEmpty, isString, map } from 'lodash';
import { IFormInputText } from '^components/atoms/form-input-text/IFormInputText';
import { IPieChartTypes } from '^components/atoms/kendo-pie-chart/ikendo-pie-chart';
import { DynamicFormComponent } from '^components/molecules/dynamic-form/dynamic-form.component';
import { EDynamicFieldType } from '^components/molecules/dynamic-form/EDyamicFieldType';
import { EModalBtnTypes } from '^components/organisms/modal/EModalBtnTypes';
import { EModalSize } from '^components/organisms/modal/EModalSize';
import { ModalService } from '^components/organisms/modal/modal.service';
import { APP } from '^utilities/constants/APP.constant';
import { CHART_COLORS, SESSION } from '^utilities/constants/DASHBOARD.constant';
import { PolicyVerifyModalComponent } from '../policy-verify-modal/policy-verify-modal.component';

enum EComponentKeys {
  CLIPBOARD = 'Clipboard',
  URL = 'URL Content Redirection',
  SMART_CARD = 'Smart Card',
  USB = 'USB'
}

const HEALTH_TAB_CONFIG = [
  {
    TITLE_KEY: 'Clipboard',
    KEY: EComponentKeys.CLIPBOARD,
    showAsPercent: true,
    isPercentValue: false
  },
  {
    TITLE_KEY: 'URL Redirection',
    KEY: EComponentKeys.URL,
    showAsPercent: true,
    isPercentValue: false
  },
  {
    TITLE_KEY: 'Smartcard Redirection',
    KEY: EComponentKeys.SMART_CARD,
    showAsPercent: true,
    isPercentValue: false
  }
];

const DEFAULT_GRID_CONFIG = {
  isSingleSelection: false,
  isMultiSelection: false,
  placeholderText: 'TRANSLATE.MESSAGES.INFO_NO_RESULTS_FOUND',
  paginationText: 'TRANSLATE.LABELS.SESSION_S',
  isFilterable: true,
  hideFooter: true,
  disablePagination: true
  // showPageSizeOptions: true,
  // pageSize: 10
};

enum EHealthDataType {
  STATUS_TRAIN,
  TOGGLE,
  GRID,
  FORM
}

@Component({
  selector: 'app-dashboard-insights-health',
  templateUrl: './dashboard-insights-health.component.html',
  styleUrls: ['./dashboard-insights-health.component.scss']
})
export class DashboardInsightsHealthComponent implements OnInit {
  @ViewChildren(DynamicFormComponent)
  forms: QueryList<any>;

  @ViewChild('statusTemplate', { static: true })
  statusTemplate;

  @ViewChild('levelTemplate', { static: true })
  levelTemplate;

  @ViewChild('urlValueTemplate', { static: true })
  urlValueTemplate;

  @ViewChild('smartCardStatusTemplate', { static: true })
  smartCardStatusTemplate;

  @Input()
  set rxHealthData(data) {
    if (!data || isEmpty(data)) {
      return;
    }
    this.processRxHealthData(data);
  }

  tabs: any[] = [];
  activeTab: any;
  chartType = IPieChartTypes.DONUT;
  series: Array<any>;
  axisDefaults: any;
  seriesDefaults: any;
  seriesColors = [CHART_COLORS.PINK, CHART_COLORS.GREEN];
  healthDataType = EHealthDataType;
  formValues: any = {};

  statusColor = SESSION.STATUS_COLORS;

  constructor(private l10n: L10nService, private modalService: ModalService) {
    this.initTrendConfigs();
  }

  ngOnInit(): void {
    // this.getHealthData();
  }

  ngAfterViewInit() {
    this.forms.forEach((form: DynamicFormComponent) => {
      form.form.valueChanges.subscribe(values => {
        this.formValues[form.id] = values;
      });
    });
  }

  initTrendConfigs() {
    this.tabs = HEALTH_TAB_CONFIG.map(config => {
      return {
        key: config.KEY,
        title: config.TITLE_KEY
      };
    });

    this.selectTab(this.tabs[0]);

    this.axisDefaults = { labels: { font: APP.FONT.DEFAULT } };

    this.seriesDefaults = { startAngle: 320, labels: { font: APP.FONT.DEFAULT } };

    this.series = [
      {
        field: 'placeholder',
        padding: 0,
        size: 30,
        visible: false
      },
      {
        field: 'value',
        categoryField: 'source',
        padding: 0,
        size: 5
      }
    ];
  }

  processRxHealthData(data) {
    const clipboardTab = this.tabs.find(tab => tab.key === EComponentKeys.CLIPBOARD);
    const clipboardHealthData = data.Clipboard;
    const clipboardOperations = data.ClipboardOps['User Operations'];
    this.processOverallHealthData(clipboardTab, clipboardHealthData);
    this.processClipboardData(clipboardTab, clipboardHealthData, clipboardOperations);

    const urlTab = this.tabs.find(tab => tab.key === EComponentKeys.URL);
    const urlHealthData = data.URLRedirect.HealthStatus[0];
    this.processOverallHealthData(urlTab, urlHealthData);
    this.processUrlData(urlTab, urlHealthData);

    const smartCardTab = this.tabs.find(tab => tab.key === EComponentKeys.SMART_CARD);
    const smartCardHealthData = data.SmartCard;
    this.processOverallHealthData(smartCardTab, smartCardHealthData);
    this.processSmartCardData(smartCardTab, smartCardHealthData);
  }

  private processOverallHealthData(tab, healthData) {
    tab.data = [
      {
        categoryField: 'source',
        value: 100,
        color: healthData['Overall Health Status'] === 'Healthy' ? CHART_COLORS.GREEN : CHART_COLORS.RED
      }
    ];
  }

  private processClipboardData(clipboardTab, clipboardHealthData, clipboardOperations) {
    const policySettingData = [clipboardHealthData.HealthStatus[0]['Policy Setting'].ClipboardState, clipboardHealthData.HealthStatus[0]['Policy Setting'].ClipboardSize];
    clipboardTab.detailData = [];
    clipboardTab.detailData.push({
      type: EHealthDataType.GRID,
      title: 'Policy Setting',
      gridConfig: {
        ...DEFAULT_GRID_CONFIG,
        columns: [
          {
            label: 'Policy Name',
            field: 'PolicyName'
          },
          {
            label: 'Registry HIVE',
            field: 'Registry HIVE'
          },
          {
            label: 'Value',
            field: 'Value'
          }
        ],
        data: policySettingData
      }
    });

    const serviceStatusData = [];
    each(clipboardHealthData.HealthStatus[0]['Service Status'], (item, key) => {
      serviceStatusData.push({
        service: key,
        status: item
      });
    });
    clipboardTab.detailData.push({
      type: EHealthDataType.GRID,
      title: 'Service Status',
      gridConfig: {
        ...DEFAULT_GRID_CONFIG,
        columns: [
          {
            label: 'Status',
            field: 'status',
            cellTemplate: this.statusTemplate
          },
          {
            label: 'Service',
            field: 'service'
          }
        ],
        data: serviceStatusData
      }
    });

    const userOperations = clipboardOperations;
    clipboardTab.detailData.push({
      type: EHealthDataType.GRID,
      title: 'User Operations',
      gridConfig: {
        ...DEFAULT_GRID_CONFIG,
        columns: [
          {
            label: 'Level',
            field: 'Level',
            cellTemplate: this.levelTemplate
          },
          {
            label: 'Description',
            field: 'Description'
          }
        ],
        data: userOperations
      }
    });
  }

  private processUrlData(urlTab, urlHealthData) {
    const tab = urlTab;

    const policySettingData = [
      urlHealthData['Policy Setting'].URLRedirectionEnabled,
      urlHealthData['Policy Setting'].URLRedirectionIPRulesEnabled,
      urlHealthData['Policy Setting'].URLRedirectionSide,
      urlHealthData['Policy Setting'].URLRedirectionProtocolHttps
    ];

    const serviceStatusData = [];
    each(urlHealthData['Service Status'], (item, key) => {
      serviceStatusData.push({
        service: key,
        status: item
      });
    });
    tab.detailData = [
      {
        type: EHealthDataType.GRID,
        title: 'Policy Setting',
        gridConfig: {
          ...DEFAULT_GRID_CONFIG,
          columns: [
            {
              label: 'Policy Name',
              field: 'PolicyName'
            },
            {
              label: 'Registry HIVE',
              field: 'Registry HIVE'
            },
            {
              label: 'Value',
              field: 'Value',
              cellTemplate: this.urlValueTemplate
            }
          ],
          data: policySettingData
        }
      },
      {
        type: EHealthDataType.GRID,
        title: 'Service Status',
        gridConfig: {
          ...DEFAULT_GRID_CONFIG,
          columns: [
            {
              label: 'Status',
              field: 'status',
              cellTemplate: this.statusTemplate
            },
            {
              label: 'Service',
              field: 'service'
            }
          ],
          data: serviceStatusData
        }
      },
      {
        type: EHealthDataType.FORM,
        title: 'Related Service/Process Status',
        id: 'relatedServiceProcessForm',
        formConfig: new BehaviorSubject([]),
        formButtons: [
          {
            label: 'Policy Verify',
            handler: () => {
              console.log('button clicked with below values:');
              console.log(this.formValues['relatedServiceProcessForm']);
              this.modalService.open({
                title: 'Policy Verify',
                // bodyText: 'Policy verify succeed',
                bodyComponent: PolicyVerifyModalComponent,
                componentData: {
                  formValues: this.formValues['relatedServiceProcessForm']
                },
                size: EModalSize.MD,
                okBtnOptions: {
                  type: EModalBtnTypes.PRIMARY,
                  text: 'TRANSLATE.LABELS.OK'
                },
                hideCancelButton: false,
                okBtnHandler: this.policyVerifyModalOKHanlder.bind(this)
              });
            }
          }
        ]
      }
    ];
  }

  private processSmartCardData(smartCardTab, smartCardHealthData) {
    delete smartCardHealthData.AgentHealthChecking.HealthStatus;
    delete smartCardHealthData.ClientHealthChecking.HealthStatus;

    let agentDescription;
    if (smartCardHealthData.AgentHealthChecking['Health Description']) {
      agentDescription = smartCardHealthData.AgentHealthChecking['Health Description'];
      delete smartCardHealthData.AgentHealthChecking['Health Description'];
    }
    let clientDescription;
    if (smartCardHealthData.ClientHealthChecking['Health Description']) {
      clientDescription = smartCardHealthData.ClientHealthChecking['Health Description'];
      delete smartCardHealthData.ClientHealthChecking['Health Description'];
    }
    const agentHealthCheckStatues = map(smartCardHealthData.AgentHealthChecking, (item, key) => {
      return {
        label: key,
        status: item.toLowerCase()
      };
    });
    const clientHealthCheckStatues = map(smartCardHealthData.ClientHealthChecking, (item, key) => {
      return {
        label: key,
        status: item.toLowerCase()
      };
    });

    const agentSmartCardData = map(smartCardHealthData.AgentSmartCardStatus, (item, key) => {
      return {
        property: key,
        status: item
      };
    });
    const clientSmartCardData = map(smartCardHealthData.ClientSmartCardStatus, (item, key) => {
      return {
        property: key,
        status: item
      };
    });
    smartCardTab.detailData = [
      {
        type: EHealthDataType.STATUS_TRAIN,
        title: 'Health Check Status',
        statusList: [
          {
            label: 'Agent Health Checking',
            statuses: agentHealthCheckStatues,
            description: agentDescription
          },
          {
            label: 'Client Health Checking',
            statuses: clientHealthCheckStatues,
            description: clientDescription
          }
        ]
      },
      {
        type: EHealthDataType.GRID,
        title: 'Agent Smart Card Status',
        gridConfig: {
          ...DEFAULT_GRID_CONFIG,
          columns: [
            {
              label: 'Property',
              field: 'property'
            },
            {
              label: 'Status',
              field: 'status',
              cellTemplate: this.smartCardStatusTemplate
            }
          ],
          data: agentSmartCardData
        }
      },
      {
        type: EHealthDataType.GRID,
        title: 'Client Smart Card Status',
        gridConfig: {
          ...DEFAULT_GRID_CONFIG,
          columns: [
            {
              label: 'Property',
              field: 'property'
            },
            {
              label: 'Status',
              field: 'status',
              cellTemplate: this.smartCardStatusTemplate
            }
          ],
          data: clientSmartCardData
        }
      }
    ];
  }

  getHealthData() {
    this.tabs.forEach(tab => {
      if (tab.key === EComponentKeys.SMART_CARD) {
        tab.data = [
          {
            categoryField: 'source',
            value: 100,
            color: CHART_COLORS.GREEN
          }
        ];
        tab.detailData = [
          {
            type: EHealthDataType.TOGGLE,
            value: false,
            title: 'Smartcard Component Deploy Status',
            label: 'On'
          },
          {
            type: EHealthDataType.STATUS_TRAIN,
            title: 'Health Check Status',
            statusList: [
              {
                label: 'Client Side Checking',
                statuses: [
                  {
                    label: 'OS Version',
                    status: 'ok'
                  },
                  {
                    label: 'Smartcard Middleware',
                    status: 'error'
                  },
                  {
                    label: 'Smartcard Configuration',
                    status: 'warning'
                  }
                ]
              }
            ]
          },
          {
            type: EHealthDataType.GRID,
            title: 'Smartcard Related Policy',
            gridConfig: {
              ...DEFAULT_GRID_CONFIG,
              columns: [
                {
                  label: 'Policy Source',
                  field: 'policySource'
                },
                {
                  label: 'Policy Name',
                  field: 'policyName'
                },
                {
                  label: 'Value',
                  field: 'value'
                },
                {
                  label: 'Registry Path',
                  field: 'registryPath'
                }
              ],
              data: [
                {
                  policySource: 'GPO'
                },
                {
                  policySource: 'DEM'
                }
              ]
            }
          }
        ];
      } else {
        tab.data = [
          {
            categoryField: 'source',
            value: 100,
            color: CHART_COLORS.GREEN
          }
        ];
        tab.detailData = [
          {
            type: EHealthDataType.FORM
          }
        ];
      }
    });
  }

  policyVerifyModalOKHanlder() {
    console.log('policyVerifyModalOKHanlder');
  }

  selectTab(tab) {
    this.tabs.forEach(tabItem => {
      tabItem.selected = tabItem.key === tab.key;

      if (tabItem.selected) {
        this.activeTab = tabItem;
      }
    });
  }

  isValueString(data) {
    return isString(data);
  }
}
