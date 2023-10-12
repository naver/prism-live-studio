# prism-chat

## 一、URL
### 1. All Page

URL: /path/to/**all**.html?lang=en

param: lang, value: ko(default) / en
  

### 2. NaverTV & Vlive Page

URL: /path/to/**naver**.html?lang=en&platform=NAVERTV

param => lang, value => ko(default) / en

param => plarform, value => VLIVE / NAVERTV


### 3. Youtube Page

URL: /path/to/**youtube**.html?lang=en

param => lang, value => ko(default) / en


### 3. AfreecaTV & Facebook Page

URL: /path/to/**mqtt**.html?lang=en&platform=FACEBOOK

param => lang, value => ko(default) / en

param => plarform, value => AFREECATV / FACEBOOK

___
  
## 二、Events
### 1. PRISM Client -> Web
```js
dispatchEvent(new CustomEvent("prism_events", {
  detail: {
    type: "init",  // "init", "end", "token", "chat", "platform_close", "broadcast"
    data: "xxx"
  }
}))
```

#### 1.1 Start live [ type: "init" ]

1.1.1 Youtube
| param | value | description
| ---- | ------| ----------
| platforms | Array[Object] |
| platforms[].name | |
| platforms[].accessToken | |

1.1.2 Twitch
| param | value | description
| ---- | ------| ----------
| platforms | Array[Object] |
| platforms[].name | |
| platforms[].clientId | |


1.1.3 NaverTV
| param | value | description
| ---- | ------| ----------
| platforms | Array[Object] |  |
| platforms[].name | |  |
| platforms[].objectId | string |
| platforms[].lang | string |
| platforms[].ticket | string |
| platforms[].templateId | string |
| platforms[].objectUrl | string |
| platforms[].GLOBAL_URI | string | api address
| platforms[].HMAC_SALT | string | hmac
| platforms[].username | string |
| platforms[].groupId | string |
| platforms[].userType | string |
| platforms[].NID_SES | string |
| platforms[].nid_inf | string |
| platforms[].NID_AUT | string |
| platforms[].userId | string |

1.1.4 VLive
| param | value | description
| ---- | ------| ----------
| platforms | Array[Object] |  |
| platforms[].name | |  |
| platforms[].accessToken | | For youtube, twitch |
| platforms[].objectId | string | 
| platforms[].lang | string | 
| platforms[].ticket | string | 
| platforms[].templateId | string | 
| platforms[].objectUrl | string | 
| platforms[].GLOBAL_URI | string | api address
| platforms[].HMAC_SALT | string | hmac
| platforms[].username | string | 
| platforms[].gcc | string | 
| platforms[].snsCode |string | 
| platforms[].pool | string | 
| platforms[].extension | object | 
| platforms[].extension.no | string | 
| platforms[].extension.cno | int | 
| platforms[].version | int | 
| platforms[].consumerKey | string | authorization
| platforms[].Authorization | string | authorization
| platforms[].isVliveFanship | boolean | true: show fanship icon
| platforms[].isRehearsal | boolean | 

1.1.5 Facebook
| param | value | description
| ---- | ------| ----------
| platforms | Array[Object] |  |
| platforms[].name | |  |
| platforms[].isPrivate | boolean |

1.1.6 AfreecaTV
| param | value | description
| ---- | ------| ----------
| platforms | Array[Object] |  |
| platforms[].name | |  |
| platforms[].userId | string |
| platforms[].isPrivate | boolean |

```js
dispatchEvent(new CustomEvent("prism_events", {
  detail: {
    type: "init",
    data: {
      platforms: [{
        name: "YOUTUBE",
        accessToken: ""
      }, {
        name: "TWITCH",
        clientId: ''
      }, {
        name: "NAVERTV",
        objectId: '',
        groupId: '',
        lang: '',
        ticket: '',
        templateId: '',
        userType: '',
        HMAC_SALT: '',
        NID_SES: '',
        nid_inf: '',
        NID_AUT: '',
        userId: '',
        username: '',
        GLOBAL_URI: '',
        objectUrl: ''
      }, {
        name: "VLIVE",
        objectId: '',
        gcc: '',
        lang: '',
        ticket: '',
        templateId: '',
        objectUrl: '',
        snsCode: '',
        pool: '',
        username: '',
        extension: {
          'no': '',
          'cno': 0
        },
        isVliveFanship: false,
        version: 1,
        GLOBAL_URI: '',
        HMAC_SALT: '',
        consumerKey: '',
        Authorization: ''
      }]
    }
  }
}))
```

#### 1.2 Finish live [ type: "end" ]
```js
dispatchEvent(new CustomEvent("prism_events", {
  detail: {
    type: "end"
  }
}))
```

#### 1.3 Refresh token [ type: "token" ]
```js
dispatchEvent(new CustomEvent("prism_events", {
  detail: {
    type: "token",
    data: {
      platform: "YOUTUBE"
      token: ""
    }
  }
}))
```

#### 1.4 Chatting message [ type: "chat" ]
```js
dispatchEvent(new CustomEvent("prism_events", {
  detail: {
    type: "chat",
    data: {
      message: '[{"livePlatform":"TWITCH","message":"test","rawMessage":"@badge-info=;badges=broadcaster/1;color=;display-name=lan;emotes=;flags=;id=7982a387-3fd4-4b5c-b8fb-889d0f61er4;mod=0;room-id=42164871;subscriber=0;tmi-sent-ts=1584324715462;turbo=0;user-id=423437451;user-type= :lan!lan@lan.tmi.twitch.tv PRIVMSG #lan :test","publishedAt":1584324788782,"author":{"profileImageUrl":"","displayName":"lan","isChatOwner":true}}]'
    }
  }
}))
```

#### 1.5 Close one of the channel during living [ type: "platform_close" ]
```js
dispatchEvent(new CustomEvent("prism_events", {
  detail: {
    type: "platform_close",
    data: {
      platform: 'YOUTUBE'
    }
  }
}))
```

#### 1.6 Rebroadcast [ type: "broadcast" ]
```js
dispatchEvent(new CustomEvent("prism_events", {
  detail: {
    type: "broadcast",
    data: {
      receive: 'ALL',
      type: 'delete',
      data: commentNo
    }
  }
}))
```

#### 1.7 Change chat permissions  [ type: "permission" ]
```js
dispatchEvent(new CustomEvent("prism_events", {
  detail: {
    type: "permission",
    data: {
      platform: 'FACEBOOK',
      isPrivate: true
    }
  }
}))
```

### 2. Web -> PRISM client
#### 2.1 Send chatting message [ type: "send" ]
```js
// send to all the active platforms
sendToPrism(JSON.stringify({
  type: "send",
  data: {
    message: "test"
  }
))
```

```js
// send to the specific platform
sendToPrism(JSON.stringify({
  type: "send",
  data: {
    message: "test",
    platform: 'AFREECATV'
  }
))
```

#### 2.2 Token expired [ type: "token" ]
```js
sendToPrism(JSON.stringify({
  type: "token",
  data: {
    platform: "YOUTUBE"
  }
))
```

#### 2.3 Request broadcasting [ type: "broadcast" ]
```js
sendToPrism(JSON.stringify({
  type: "broadcast",
  data: {
    receive: 'ALL',
    type: 'delete',
    data: commentNo
  }
))
```
