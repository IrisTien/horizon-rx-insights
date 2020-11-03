#-*- coding: UTF-8 -*-
import re
import sys
import json

def CheckURLMatchRules(testUrl, rules):
    result = False
    testRulesResult = {'SideMatchResult': False, 'eachRuleResult': {}}
    if rules == '':
        return testRulesResult
    rulesArray = rules.split(';')
    Data = []
    for a in rulesArray:
        test = re.search(a, testUrl)
        if test is None:
            tempData = {'rule': a, 'status': False}
            Data.append(tempData)
        else:
            tempData = {'rule': a, 'status': True}
            Data.append(tempData)
            testRulesResult['SideMatchResult'] = True
    testRulesResult['eachRuleResult'] = Data
    return testRulesResult

def ChooseLocalRemote(local, remote):
    if remote == False or local == True:
        return False
    return True

def CheckUrlRedirected(testUrl, clientRules, agentRules, side):
    resultClientRules = CheckURLMatchRules(testUrl, clientRules)
    resultAgentRules = CheckURLMatchRules(testUrl, agentRules)
    IsRedirected = []
    if side == 'Client':
        IsRedirected = ChooseLocalRemote(resultClientRules['SideMatchResult'], resultAgentRules['SideMatchResult'])
    else:
        IsRedirected = ChooseLocalRemote(resultAgentRules['SideMatchResult'], resultClientRules['SideMatchResult'])
    ResultDict = {}
    ResultDict['testUrl'] = testUrl
    ResultDict['IsRedirected'] = IsRedirected
    ResultDict['clientRules'] = resultClientRules
    ResultDict['agentRules'] = resultAgentRules

    if ResultDict['IsRedirected'] == True:
        ResultDict['IsRedirected'] = 'Redirect the URL from ' + side
    else:
        ResultDict['IsRedirected'] = 'Not Redirect the URL from ' + side

    if ResultDict['clientRules']['SideMatchResult'] == True:
        ResultDict['clientRules']['SideMatchResult'] = 'Matched'
    else:
        ResultDict['clientRules']['SideMatchResult'] = 'MisMatched'

    if ResultDict['agentRules']['SideMatchResult'] == True:
        ResultDict['agentRules']['SideMatchResult'] = 'Matched'
    else:
        ResultDict['agentRules']['SideMatchResult'] = 'MisMatched'

    return ResultDict

def CheckUrl(text):
    testUrl = text['testUrl']
    clientRules = text['clientRules']
    agentRules = text['agentRules']
    side = text['side']
    response = CheckUrlRedirected(testUrl, clientRules, agentRules, side)
    return json.dumps(response)