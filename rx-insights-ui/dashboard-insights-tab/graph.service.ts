import { Injectable } from '@angular/core';
import { toArray, isArray, forEach } from 'lodash';
import { INSIGHTS as APP } from '^utilities/constants/DASHBOARD.constant';

@Injectable({
  providedIn: 'root'
})
export class GraphService {
  constructor() {}

  generateRelationships(queryString) {
    let _start = 0;
    let _end = 0;
    let i = -1;

    const _links = [];

    const _objectStr = queryString.substring(queryString.indexOf('return ') + 7);
    const _objectArr = _objectStr.split(/[\s,;]+/).filter(item => item !== '');

    while (_start != -1) {
      _start = queryString.indexOf('[:', i + 1);
      _end = queryString.indexOf(']', i + 1);

      if (_start !== -1 && _end !== -1) {
        const _relation = queryString.substring(_start + 2, _end);

        const _link: any = {};
        _link.relationship = APP.RELATION_MAP[_relation].RELATION;

        _link.from = _objectArr.indexOf(APP.RELATION_MAP[_relation].FROM);
        _link.to = _objectArr.indexOf(APP.RELATION_MAP[_relation].TO);

        _links.push(_link);

        _start += _relation.length + 3;
      }

      i = _start;
    }

    return _links;
  }

  generateGridData(dataArr) {
    const _gridDataObj = {};
    dataArr.forEach(dataGroup => {
      dataGroup.forEach(data => {
        if (isArray(data)) {
          // Relationship array
          // const _relationArr = data;
        } else if (data.type) {
          // Relationship
          // const _relation = data;
        } else {
          // Node data
          const _node = data;
          if (_node.labels && _node.labels.length && _node.labels[0]) {
            const objName = _node.labels[0];
            if (!_gridDataObj[objName]) {
              _gridDataObj[objName] = {
                columns: [],
                data: []
              };
            }
            // if (objName === 'Person') {
            //   _gridDataObj[objName].type = 'Summary';
            // }
            const _dataItem: any = {};
            if (!_gridDataObj[objName].data.find(item => item.id === _node.identity)) {
              _dataItem.id = _node.identity;
              forEach(_node.properties || {}, (value, key) => {
                if (_gridDataObj[objName].columns.indexOf(key) === -1) {
                  _gridDataObj[objName].columns.push(key);
                }
                _dataItem[key] = value;
              });

              _gridDataObj[objName].data.push(_dataItem);
            }
          }
        }
      });
    });
    return _gridDataObj;
  }
}
