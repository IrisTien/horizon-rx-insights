import { Component, OnInit } from '@angular/core';
import * as ZoomChart from '@dvsl/zoomcharts';
import { forEach, isArray, isNumber, map as mapToArray, uniqBy } from 'lodash';
import { BehaviorSubject, of } from 'rxjs';
import { catchError, map, switchMap } from 'rxjs/operators';
import { HydraService } from '^services/api/hydra/hydra.service';
import { APP as APPCONST } from '^utilities/constants/APP.constant';
import { INSIGHTS as APP } from '^utilities/constants/DASHBOARD.constant';
import { EnvironmentService } from '^utilities/environment/environment.service';
import { CommonHelperService } from '^utilities/helpers/common-helper.service';
import { DashboardSessionListService } from '../dashboard-overview-tab/dashboard-session-list/dashboard-session-list.service';
import { Neo4jService } from '../neo4j.service';
import { GraphService } from './graph.service';
import { RxService } from './rx.service';

const NODE_RELATIONSHIP = {
  USER: ['CLIENT'],
  CLIENT: ['SMARTCARD', 'PRINTER', 'DESKTOP'],
  DESKTOP: ['POOL'],
  POOL: ['BROKER']
};

const NODE_CONFIG = {
  ClientMachineName: {
    type: 'CLIENT',
    label: 'Client'
  },
  User: {
    type: 'USER',
    label: 'User'
  },
  smartCard: {
    type: 'SMARTCARD',
    label: 'Smart Card'
  },
  printer: {
    type: 'PRINTER',
    label: 'Printer'
  },
  ClientLaunchId: {
    type: 'DESKTOP',
    label: 'Desktop'
  },
  FarmId: {
    type: 'POOL',
    label: 'Pool'
  },
  BrokerDns: {
    type: 'BROKER',
    label: 'Connection Server'
  }
};

@Component({
  selector: 'app-dashboard-insights-tab',
  templateUrl: './dashboard-insights-tab.component.html',
  styleUrls: ['./dashboard-insights-tab.component.scss']
})
export class DashboardInsightsTabComponent implements OnInit {
  noData = true;
  loading = false;
  showError = false;
  showHealth = false;
  tooltipData = [];
  gridDataObj = {};
  gridKeys = [];
  chartMode = APP.MODE.HIERARCHY;
  nodeLegends = [];
  rxHealthData: any;

  sessionInfoSubject = new BehaviorSubject(undefined);

  private zoomChart: any = ZoomChart;
  private processedChartData = {};

  constructor(
    private neo4jService: Neo4jService,
    private graphService: GraphService,
    private rxService: RxService,
    private hydraService: HydraService,
    private env: EnvironmentService,
    private sessionListService: DashboardSessionListService,
    private commonHelperService: CommonHelperService
  ) {}

  ngOnInit(): void {
    this.getOverallView();
    // this.getSessionID();

    this.sessionInfoSubject.subscribe(session => {
      if (session) {
        this.getAllDataFromRx(session.podId, session.id);
      }
    });
  }

  processCommonData(nodesArr, relationShips?) {
    const _data = { nodes: [], links: [] };
    this.nodeLegends = [];
    nodesArr.forEach(nodeGroup => {
      // relationShips.forEach(relation => {
      //   const _link: any = {};
      //   _link.id = `${nodeGroup[relation.from].identity}-${nodeGroup[relation.to].identity}`
      //   _link.from = nodeGroup[relation.from].identity.toString();
      //   _link.to = nodeGroup[relation.to].identity.toString();
      //   _link.style = {};
      //   _link.style.label = relation.relationship;
      //   // _link.style.length = 5;

      //   _data.links.push(_link);
      // });

      nodeGroup.forEach(node => {
        if (isArray(node)) {
          const _relationArr = node;
          _relationArr.forEach(_relation => {
            const _link: any = {};
            _link.style = {};
            _link.style.label = APP.RELATIONSHIP_MAP[_relation.type];
            _link.from = _relation.start.low.toString();
            _link.to = _relation.end.low.toString();
            _link.id = `${_link.from}-${_link.to}`;

            _data.links.push(_link);
          });
        } else if (node.type) {
          // It is relationship array
          const _relation = node;
          const _link: any = {};
          _link.style = {};
          _link.style.label = APP.RELATIONSHIP_MAP[_relation.type];
          _link.from = _relation.start.toString();
          _link.to = _relation.end.toString();
          _link.id = `${_link.from}-${_link.to}`;

          _data.links.push(_link);
        } else {
          const _node: any = {};
          _node.id = node.identity.toString();
          _node.label = node.labels[0];
          _node.style = {};
          _node.extra = [];

          forEach(node.properties || {}, (value, key) => {
            const _item: any = {};
            _item.label = key;
            _item.content = value;
            _node.extra.push(_item);
          });

          if (node.labels && node.labels.length && APP.OBJECT_IMAGES[node.labels[0].toUpperCase()]) {
            const _nodeType = node.labels[0].toUpperCase();

            if (!this.nodeLegends.find(item => item.className === node.labels[0])) {
              this.nodeLegends.push({
                className: node.labels[0],
                style: {
                  image: APP.BASE_IMAGE_PATH + APP.OBJECT_IMAGES[_nodeType]
                }
              });
            }

            if (node.properties && node.properties.imageURL) {
              _node.style.image = node.properties.imageURL;
            } else {
              const _imageFile = APP.OBJECT_IMAGES[_nodeType];
              if (_imageFile) {
                _node.style.image = APP.BASE_IMAGE_PATH + _imageFile;
              }
            }

            if (APP.OBJECT_RADIUS[_nodeType]) {
              const _radiusObj = APP.OBJECT_RADIUS[_nodeType];
              if (isNumber(_radiusObj)) {
                _node.style.radius = _radiusObj;
              } else if (_radiusObj) {
                const _fieldValue = node.properties[_radiusObj.field];
                const _radius = _radiusObj.radius[_fieldValue];
                _node.style.radius = _radius;
              }
            }

            if (APP.OBJECT_COLORS[_nodeType]) {
              _node.style.fillColor = APP.OBJECT_COLORS[_nodeType];
            }

            if (APP.OBJECT_LABELS[_nodeType]) {
              _node.style.label = node.properties && node.properties[APP.OBJECT_LABELS[_nodeType]];
            }
          }

          _data.nodes.push(_node);
        }
      });
    });
    return _data;
  }

  genChart(data?, mode?) {
    const infoElement: any = document.getElementById('tooltip');

    const chartContainer = document.getElementById('chart');
    disposeDemo();

    var t = new this.zoomChart.NetChart({
      container: chartContainer,
      area: { height: this.getHeight(), width: this.getWidth() },
      layout: {
        // groupSpacing: 50
        mode: mode ? mode : APP.MODE.DYNAMIC,
        rowSpacing: 70
      },
      legend: { enabled: true },
      data: [
        {
          preloaded: data
        }
      ],
      style: {
        // nodeClasses: this.nodeLegends,
        node: {
          labelStyle: {
            // scaleWithSize: false,
            textStyle: {
              font: '8px Metropolis,"Avenir Next","Helvetica Neue",Arial,sans-serif'
            }
          }
        },
        nodeLabel: {
          textStyle: {
            font: '10px Metropolis,"Avenir Next","Helvetica Neue",Arial,sans-serif'
          }
        },
        // nodeDetailMinSize: 10,
        // nodeDetailMinZoom: 1,
        linkLabel: {
          rotateWithLink: true,
          textStyle: {
            font: '8px Metropolis,"Avenir Next","Helvetica Neue",Arial,sans-serif'
          }
          // scaleWithSize: true,
          // scaleWithZoom: false
        },
        // linkLabelScaleBase: 0.5,
        // linkLengthExtent: [0.5, 150],
        nodeStyleFunction: this.nodeStyleHandler,
        linkStyleFunction: this.linkStyleHandler
      },
      // navigation:{mode:"focusnodes"},
      // interaction: { selection: { lockNodesOnMove: false } }
      interaction: {
        selection: {
          lockNodesOnMove: false
        },
        zooming: {
          // autoZoomExtent: [0.1, 4]
        }
      },
      events: {
        onHoverChange: (event, args) => {
          var content = '';
          // fill the info popup based on the node that was hovered.
          if (args.hoverItem) {
            content = 'Item hovered';
          } else if (args.hoverNode) {
            this.tooltipData = args.hoverNode.data && args.hoverNode.data.extra;
            content = 'Node hovered';
          } else if (args.hoverLink) {
            content = 'Link hovered';
          }

          const infoElementVisible = !!content && !!this.tooltipData;
          infoElement.style.display = infoElementVisible ? 'block' : 'none';
        },
        onClick: (event, args) => {
          if (args.hoverNode) {
            console.log(args.hoverNode);
            // if (args.hoverNode.data.label === 'Person') {
            //   // Search neo4j for this person
            //   const _personName = args.hoverNode.label;
            //   // this.onSearchTextChange.emit(_personName);
            //   this.query(_personName);
            // } else if (args.hoverNode.data.label === 'Bug') {
            //   const _bugQueryText = `Bug ${args.hoverNode.label}`;
            //   // this.onSearchTextChange.emit(_bugQueryText);
            //   this.query(_bugQueryText);
            // } else {
            //   const _extraData = args.hoverNode.data && args.hoverNode.data.extra;
            //   const _link = _extraData.find(item => item.label === 'link');
            //   if (_link) {
            //     window.open(_link.content, '_blank');
            //   }
            // }
            if (args.hoverNode.data.type === 'SESSION') {
              this.getSessionID();
            }
            console.log('hover node is clicked');
          }
        }
      }
    });

    function movePopup(event) {
      infoElement.style.top = event.pageY + 'px';
      infoElement.style.left = event.pageX + 'px';
    }

    // attach event handlers that move the info element with the mouse cursor.
    chartContainer.addEventListener('mousemove', movePopup, true);
    chartContainer.addEventListener('pointermove', movePopup, true);

    // function should be called whenever the chart is removed
    function disposeDemo() {
      chartContainer.removeEventListener('mousemove', movePopup);
      chartContainer.removeEventListener('pointermove', movePopup);

      // remove the menu element that was created dynamically
      if (infoElement) {
        infoElement.style.display = 'none';
      }
    }
  }

  getOverallView() {
    this.loading = true;
    this.showError = false;
    let index = 0;
    let nodes = [];
    let links = [];
    this.rxService.getRXOverallData().subscribe((overallData: any) => {
      overallData.nodes.forEach(node => {
        index++;
        const podNode = {
          id: `Node-${index}`,
          loaded: true,
          type: 'POD',
          style: {
            label: node.name,
            image: APP.BASE_IMAGE_PATH + APP.OBJECT_IMAGES.POD
          },
          extra: [
            {
              label: 'Name',
              content: node.name
            }
          ]
        };
        nodes.push(podNode);

        node.connectionServers.forEach(cs => {
          index++;
          const csNode = {
            id: `Node-${index}`,
            loaded: true,
            type: 'BROKER',
            style: {
              label: cs.serverName,
              image: APP.BASE_IMAGE_PATH + APP.OBJECT_IMAGES.BROKER
            },
            extra: [
              {
                label: 'Name',
                content: cs.serverName
              }
            ]
          };
          nodes.push(csNode);
          links.push({
            id: `link${index}`,
            from: podNode.id,
            to: csNode.id,
            style: { fillColor: 'red', toDecoration: 'arrow' }
          });
        });

        node.pools.forEach(pool => {
          index++;
          const poolNode = {
            id: `Node-${index}`,
            loaded: true,
            type: 'POOL',
            style: {
              label: pool.pool,
              image: APP.BASE_IMAGE_PATH + APP.OBJECT_IMAGES.POOL
            },
            extra: [
              {
                label: 'Name',
                content: pool.pool
              }
            ]
          };
          nodes.push(poolNode);
          index++;
          links.push({
            id: `link${index}`,
            from: podNode.id,
            to: poolNode.id,
            style: { fillColor: 'red', toDecoration: 'arrow' }
          });

          const sessionNumber = Math.ceil(Math.random());
          for (let i = 0; i < sessionNumber; i++) {
            index++;
            const sessionNode = {
              id: `Session-${index}`,
              loaded: true,
              type: 'SESSION',
              style: {
                label: 'Session',
                image: APP.BASE_IMAGE_PATH + APP.OBJECT_IMAGES.SESSION
              }
            };
            nodes.push(sessionNode);
            index++;
            links.push({
              id: `link${index}`,
              from: poolNode.id,
              to: sessionNode.id,
              style: { fillColor: 'red', toDecoration: 'arrow' }
            });
          }
        });
      });

      const data = {
        nodes: nodes,
        links: links
      };

      const uniqNodes = uniqBy(data.nodes, 'type');
      this.nodeLegends = [];
      uniqNodes.forEach(node => {
        this.nodeLegends.push({
          className: APP.OBJECT_LABELS[node.type],
          style: {
            image: APP.BASE_IMAGE_PATH + APP.OBJECT_IMAGES[node.type]
          }
        });
      });
      this.processedChartData = data;
      this.genChart(this.processedChartData, this.chartMode);
      this.noData = false;
      this.loading = false;
    });
  }

  getDataFromRxApi(sessionInfo, healthInfo) {
    this.loading = true;
    this.showError = false;
    this.clearChart();

    const userData: any = {
      nodes: this.generateNodes(sessionInfo, healthInfo)
    };
    userData.links = this.generateRelationships(userData.nodes, NODE_RELATIONSHIP);
    const uniqNodes = uniqBy(userData.nodes, 'type');
    this.nodeLegends = [];
    uniqNodes.forEach(node => {
      this.nodeLegends.push({
        className: APP.OBJECT_LABELS[node.type],
        style: {
          image: APP.BASE_IMAGE_PATH + APP.OBJECT_IMAGES[node.type]
        }
      });
    });
    this.processedChartData = userData;
    this.genChart(this.processedChartData, this.chartMode);
    // this.processGridData(resp);
    this.noData = false;
    this.loading = false;
  }

  private generateNodes(sessionInfo, healthInfo) {
    const smartCard = healthInfo.SmartCard.AgentSmartCardStatus['Card Name'];
    const data = {
      ...sessionInfo,
      smartCard: smartCard,
      printer: healthInfo.DeviceList.SPPrintersDataType._name
    };
    const nodes = [];
    let index = 0;
    forEach(data, (item, key) => {
      const config = NODE_CONFIG[key];
      if (config) {
        index++;
        nodes.push({
          id: `Node-${index}`,
          loaded: true,
          type: config.type,
          style: {
            label: item,
            image: APP.BASE_IMAGE_PATH + APP.OBJECT_IMAGES[config.type]
          },
          extra: [
            {
              label: 'Name',
              content: item
            }
          ]
        });
      }
    });
    return nodes;
  }

  private generateRelationships(nodes, relationships) {
    const links = [];
    let index = 0;
    nodes.forEach(node => {
      const relatedTypes = relationships[node.type] || [];
      relatedTypes.forEach(relatedType => {
        const relatedNodes = nodes.filter(node => node.type === relatedType);
        relatedNodes.forEach(relatedNode => {
          index++;
          links.push({
            id: `link${index}`,
            from: node.id,
            to: relatedNode.id,
            style: { fillColor: 'red', toDecoration: 'arrow' }
          });
        });
      });
    });
    return links;
  }

  getSessionID() {
    const token = this.env.decodedHydraAuthToken;
    const tenantId = token['tenantId'];
    this.hydraService.getSmartNodes(tenantId).subscribe(pods => {
      const horizonPods = pods.filter(item => item.type === APPCONST.POD_TYPE.VIEW || item.type === APPCONST.POD_TYPE.VIEW_VMC);

      let data = [];
      this.sessionListService.sessionSubject.next([]);
      this.sessionListService.sessionSubject.subscribe(sessions => {
        data = this.sessionListService.sortDataBySessionStatus(sessions.concat(data));
        console.log(data);
        const session = data.find(
          session =>
            session.poolOrFarmName.toUpperCase() === 'Win10-2004-VDI-1'.toUpperCase() &&
            session.userName.toUpperCase() === 'viewbj.com\\Administrator'.toUpperCase()
        );
        if (session) {
          this.sessionInfoSubject.next({
            id: session.id,
            podId: session.podId
          });
        }
      });

      this.sessionListService.getHorizonSessions(horizonPods);
    });
  }

  getAllDataFromRx(podId, sessionId) {
    console.log(sessionId);
    console.log(podId);
    this.rxService
      .getRXDataFromHelpdesk(podId, sessionId)
      .pipe(
        map((resp: any) => {
          console.log('helpdesk client info');
          console.log(resp);
          const base64data = atob(resp.ticket);
          console.log(base64data);
          const dataInJson = JSON.parse(base64data);
          console.log(dataInJson);
          this.getDataFromRxApi(dataInJson.session_info, dataInJson.health_info);
          return dataInJson.health_info;
        }),
        catchError(() => {
          return of({});
        })
      )
      .subscribe(data => {
        console.log('rx health data');
        console.log(data);
        this.rxHealthData = data;
        this.showHealth = true;
      });
  }

  query(text) {
    this.loading = true;
    this.showError = false;
    this.clearChart();

    const _trimedText = text.trim();
    if (_trimedText.toUpperCase().indexOf('MATCH') === 0) {
      this.doNeo4jQuery(text);
    } else {
      this.neo4jService.findQueryString(text).subscribe(
        queryString => {
          this.doNeo4jQuery(queryString);
        },
        error => {
          const _query = error.error.text;
          if (_query && _query.toUpperCase() !== 'ERROR') {
            this.doNeo4jQuery(_query);
          } else {
            this.doErrorHandling();
          }
        }
      );
    }
  }

  doErrorHandling() {
    this.showError = true;
    this.loading = false;
    this.noData = false;
  }

  doNeo4jQuery(text) {
    this.neo4jService
      .query(text)
      .then(resp => {
        this.processedChartData = this.processCommonData(resp);
        this.genChart(this.processedChartData, this.chartMode);
        this.processGridData(resp);
        this.noData = false;
        this.loading = false;
      })
      .catch(err => {
        this.doErrorHandling();
      });
  }

  processGridData(resp) {
    this.gridDataObj = {};
    this.gridDataObj = this.graphService.generateGridData(resp);
    this.gridKeys = Object.keys(this.gridDataObj || {});
  }

  clearChart() {
    const chartContainer = document.getElementById('chart');
    while (chartContainer.children && chartContainer.children.length) {
      chartContainer.children[0].remove();
    }
  }

  nodeStyleHandler(node) {
    // if (node.data.label === 'Person' || node.data.label === 'Customer' ||
    // node.data.label === 'Bug') {
    // node.display = 'text';
    node.imageCropping = 'crop';
    // node.imageSlicing = [0,0,64,64];
    // }
    // if (node.data.type === 'person') {
    // node.className = 'icon-class';
    // node.id = node.data.identity;
    // node.label = node.data.labels[0];
    // }
    // if(!node.data.fillColor) {
    //   var color = sliceColors[Math.floor(Math.random()*sliceColors.length)];

    //   node.fillColor = node.data.fillColor = color;
    //   node.lineColor = node.data.lineColor =color.replace(",1)",",0.5)");
    //   node.lineWidth = node.data.lineWidth = 8;
    // } else {
    //   node.fillColor = node.data.fillColor;
    //   node.lineColor = node.data.lineColor;
    //   node.lineWidth = node.data.lineWidth;
    // }
  }

  linkStyleHandler(link) {
    // link.fromDecoration = "circle";
    link.toDecoration = 'arrow';
    link.radius = 3;
    link.fillColor = '#ccc';
    // link.fillColor = 'rgba(236,46,46,1)';
  }

  onModeChange(mode) {
    this.clearChart();
    this.genChart(this.processedChartData, mode);
  }

  private getWidth() {
    return (
      Math.max(
        document.body.scrollWidth,
        document.documentElement.scrollWidth,
        document.body.offsetWidth,
        document.documentElement.offsetWidth,
        document.documentElement.clientWidth
      ) -
      144 -
      180
    );
  }

  private getHeight() {
    return (
      Math.max(
        document.body.scrollHeight,
        document.documentElement.scrollHeight,
        document.body.offsetHeight,
        document.documentElement.offsetHeight,
        document.documentElement.clientHeight
      ) -
      216 -
      200
    );
  }
}
