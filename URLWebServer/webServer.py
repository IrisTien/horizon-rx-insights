#!/usr/bin/python

from flask import Flask, request, jsonify
from flask_cors import CORS, cross_origin
#from py2neo import Graph
import senmantic_query
#Import testUrl
import json
import urllib.parse

app = Flask(__name__)
#cors = CORS(app, supports_credentials=True)
cors = CORS(app)
#app.config[]

def queryWithPara(parm):

    if parm is None:
        return ''
    Data = urllib.parse.unquote(parm)
    #result = graph.run(cql).data()
    #result = graph.run(parm).data()
    return Data

@app.route("/", methods=["GET", "POST"])
def handleQuery():
    try:
        print(request.form)
        queryContent = request.args.get('agentRules') 
       #queryContent = request.form['query']
    except:
        return 'No query'

#    if queryContent is None:
#        return "Please input query string"
#    else:
#        print('hello world')
        """
        return queryWithPara(quer
                        queryPara = request.args.get('query')

        """
    
    # Create the Data for filtering
    TestData = {'agentRules': '', 'clientRules': '', 'testUrl': '', 'side': ''}
    TestData['agentRules'] = queryWithPara(request.args.get('agentRules'))
    TestData['clientRules'] = queryWithPara(request.args.get('clientRules'))
    TestData['testUrl'] = queryWithPara(request.args.get('testUrl'))
    TestData['side'] = queryWithPara(request.args.get('side'))

    # Check the filter data
    if TestData['side'] != 'Client' and TestData['side'] != 'Agent':
        return 'Please input the right side Type'
    if TestData['testUrl'] == '':
        return 'Please input the test URL'

    print(request.args.get('clientRules'))
    print(request.args.get('testUrl'))
    print(TestData)
    response = jsonify(senmantic_query.CheckUrl(TestData))
    #response.headers.add('Access-Control-Allow-Origin', '*')
    return response

if __name__ == "__main__":
    app.run(host='0.0.0.0', debug=True)
