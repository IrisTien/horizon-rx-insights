import { Component, OnInit, ViewChild } from '@angular/core';
import { BehaviorSubject } from 'rxjs';
import { IFormInputSelect2 } from '^components/atoms/form-input-select2/IFormInputSelect2';
import { IFormInputText } from '^components/atoms/form-input-text/IFormInputText';
import { DynamicFormComponent } from '^components/molecules/dynamic-form/dynamic-form.component';
import { EDynamicFieldType } from '^components/molecules/dynamic-form/EDyamicFieldType';
import { SESSION } from '^utilities/constants/DASHBOARD.constant';
import { RxService } from '../rx.service';

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

@Component({
  selector: 'app-policy-verify-modal',
  templateUrl: './policy-verify-modal.component.html',
  styleUrls: ['./policy-verify-modal.component.scss']
})
export class PolicyVerifyModalComponent implements OnInit {
  @ViewChild(DynamicFormComponent)
  form: DynamicFormComponent;

  @ViewChild('statusTemplate', { static: true })
  statusTemplate;

  policyConfig: any;
  formValues: any;
  resultVisible = false;
  resultKeyValues: any[];

  clientRuleGridConfig: any;
  agentRuleGridConfig: any;

  statusColor = SESSION.STATUS_COLORS;

  constructor(private rxService: RxService) {}

  ngAfterViewInit() {
    this.form.form.valueChanges.subscribe(values => {
      this.formValues = values;
    });
  }

  ngOnInit(): void {
    this.policyConfig = new BehaviorSubject([
      <IFormInputText>{
        type: EDynamicFieldType.TEXT,
        key: 'clientUrl',
        label: 'Client URL',
        showRequiredLabel: true,
        maxLength: 64,
        value: 'vmware.com;.*.testdell.com',
        validators: [{ required: true }],
        errorMessages: {
          required: 'TRANSLATE.MESSAGES.INFO_REQUIRED_FIELD'
        }
      },
      <IFormInputText>{
        type: EDynamicFieldType.TEXT,
        key: 'agentUrl',
        label: 'Agent URL',
        showRequiredLabel: true,
        maxLength: 64,
        value: 'baidu.com;chinanihao.cc',
        validators: [{ required: true }],
        errorMessages: {
          required: 'TRANSLATE.MESSAGES.INFO_REQUIRED_FIELD'
        }
      },
      <IFormInputText>{
        type: EDynamicFieldType.TEXT,
        key: 'url',
        label: 'Input URL',
        showRequiredLabel: true,
        maxLength: 64,
        value: 'http://www.baidu.com:6000',
        validators: [{ required: true }],
        errorMessages: {
          required: 'TRANSLATE.MESSAGES.INFO_REQUIRED_FIELD'
        }
      },
      <IFormInputSelect2>{
        type: EDynamicFieldType.SELECT2,
        key: 'side',
        name: 'side',
        label: 'Side',
        showRequiredLabel: true,
        options: {
          multiple: false,
          tags: false
        },
        isHidden: false,
        data: [
          {
            id: 'Client',
            text: 'Client'
          },
          {
            id: 'Agent',
            text: 'Agent'
          }
        ],
        value: 'Client'
      }
    ]);

    this.clientRuleGridConfig = {
      ...DEFAULT_GRID_CONFIG,
      columns: [
        {
          label: 'Status',
          field: 'status',
          cellTemplate: this.statusTemplate
        },
        {
          label: 'Client Rules',
          field: 'rule'
        }
      ],
      data: []
    };

    this.agentRuleGridConfig = {
      ...DEFAULT_GRID_CONFIG,
      columns: [
        {
          label: 'Status',
          field: 'status',
          cellTemplate: this.statusTemplate
        },
        {
          label: 'Agent Rules',
          field: 'rule'
        }
      ],
      data: []
    };
  }

  verifyPolicy() {
    this.resultVisible = false;
    this.rxService
      .verifyPolicy(this.formValues.side, this.formValues.agentUrl, this.formValues.clientUrl, this.formValues.url)
      .subscribe((policy: string) => {
        console.log('policy');
        console.log(policy);
        const policyData = JSON.parse(policy);
        this.resultKeyValues = [
          {
            key: 'Check Client Rules',
            value: policyData.clientRules.SideMatchResult.toString()
          },
          {
            key: 'Check Agent Rules',
            value: policyData.agentRules.SideMatchResult.toString()
          },
          {
            key: 'Final Result',
            value: policyData.IsRedirected.toString()
          }
        ];
        this.clientRuleGridConfig.data = policyData.clientRules.eachRuleResult;
        this.agentRuleGridConfig.data = policyData.agentRules.eachRuleResult;
        this.resultVisible = true;
      }, () => {
        this.resultVisible = true;
      });
    console.log(this.formValues);
  }
}
