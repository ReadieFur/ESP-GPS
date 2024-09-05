#pragma once

#include <Client.h>
#include <WString.h>
#include <map>
#include <ArduinoHttpClient.h>
#include <URLEncoder.h>

class HTTP
{
private:
    String BuildQueryString(std::map<String, String> query)
    {
        String queryString = "";

        if (query.empty())
            return queryString;

        for (auto &&kvp : query)
        {
            queryString += queryString.isEmpty() ? '?' : '&';
            queryString += kvp.first;

            if (kvp.second != nullptr && !kvp.second.isEmpty())
            {
                queryString += '=';
                queryString += kvp.second;
            }

            queryString += '&';
        }

        while (queryString.length() > 0 && queryString[queryString.length() - 1] == '&')
            queryString.remove(queryString.length() - 1);

        return queryString;
        // return URLEncoder.encode(queryString);
    }
    
public:
    enum SMethod
    {
        GET,
        POST,
        PUT,
        PATCH,
        DELETE
    };

    struct SRequest
    {
        SMethod method;
        String path;
        std::map<String, String> headers = {};
        std::map<String, String> query = {};
        String requestContentType = "";
        String requestBody = "";
        int responseCode;
        String responseBody;
    };

    HttpClient* ClientHttp;

    HTTP(Client& client, const char* address, uint16_t port = 80)
    {
        ClientHttp = new HttpClient(client, address, port);
    }

    ~HTTP()
    {
        delete ClientHttp;
    }

    int ProcessRequest(SRequest& request)
    {
        ClientHttp->beginRequest();
        for (auto &&kvp : request.headers)
            ClientHttp->sendHeader(kvp.first, kvp.second);
        String path = request.path + BuildQueryString(request.query);

        #ifdef DEBUG
        Serial.print("Sending request to: ");
        Serial.println(path);
        #endif

        switch (request.method)
        {
        case SMethod::GET:
            ClientHttp->get(path);
            break;
        case SMethod::POST:
            ClientHttp->post(path, request.requestContentType, request.requestBody);
            break;
        case SMethod::PUT:
            ClientHttp->put(path, request.requestContentType, request.requestBody);
            break;
        case SMethod::PATCH:
            ClientHttp->put(path, request.requestContentType, request.requestBody);
            break;
        case SMethod::DELETE:
            ClientHttp->del(path, request.requestContentType, request.requestBody);
            break;
        }

        ClientHttp->endRequest();

        request.responseCode = ClientHttp->responseStatusCode();
        request.responseBody = ClientHttp->responseBody();
        return request.responseCode;
    }
};
