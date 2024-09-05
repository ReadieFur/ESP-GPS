#pragma once

#include <Client.h>
#include <WString.h>
#include <map>
#include <ArduinoHttpClient.h>

class HTTP
{
private:
    String BuildQueryString(std::map<const char*, const char*> query)
    {
        String queryString = "";

        if (query.empty())
            return queryString;

        for (auto &&kvp : query)
        {
            queryString += queryString.isEmpty() ? '?' : '&';
            queryString += kvp.first;

            if (kvp.second != nullptr && strlen(kvp.second) != 0)
            {
                queryString += '=';
                queryString += kvp.second;
            }

            queryString += '&';
        }

        while (queryString.length() > 0 && queryString[queryString.length() - 1] == '&')
            queryString.remove(queryString.length() - 1);

        return queryString;
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
        const char* path;
        std::map<const char*, const char*> headers;
        std::map<const char*, const char*> query;
        const char* requestContentType = "";
        const char* requestBody = "";
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
        const char* path = (String(request.path) + BuildQueryString(request.query)).c_str();

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
