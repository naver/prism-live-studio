import { CHANNEL } from '@/assets/js/constants'

window.sendToPrism = function() {}

let panel = initSettingPanel()

const settingBtn = appendButton('settings', document.body)
settingBtn.style.position = 'absolute'
settingBtn.style.bottom = '80px'
settingBtn.style.right = '20px'
settingBtn.onclick = () => {
  panel.style.display = panel.style.display === 'block' ? 'none' : 'block'
}

window.sendToPrism = sendToPrism
function sendToPrism(data) {
  data = JSON.parse(data)
  switch (data.type) {
    case 'send':
      window.dispatchEvent(new CustomEvent('prism_events', {
        detail: {
          type: 'chat',
          data: {
            message: Object.values(CHANNEL).map((el) => generateTestSpecifiedData(data.data.message, el))
          }
        }
      }))
      break
    default:
      break
  }
}

// start living
const initBtn = appendButton('Go live')
initBtn.onclick = () => {
  window.dispatchEvent(new CustomEvent("prism_events", {
    detail: {
      type: "init",
      data: {
        hasRtmp: true,
        platforms: [{
          name: "YOUTUBE",
          accessToken: ""
        }, {
          name: "TWITCH",
          accessToken: ""
        }, {
          name: "NAVERTV",
          groupId: '',
          lang: '',
          ticket: '',
          templateId: '',
          userType: '',
          URI: '',
          HMAC_SALT: '',
          objectId: '',
          NID_SES: '',
          nid_inf: '',
          NID_AUT: ''
        }]
      }
    }
  }))
}

// finish living
const endBtn = appendButton('Finish live')
endBtn.onclick = () => {
  dispatchEvent(new CustomEvent("prism_events", {
    detail: {
      type: "end"
    }
  }))
}

// receive new message
const sendBtn = appendButton('Add chat message (1~5)')
sendBtn.onclick = () => {
  window.dispatchEvent(new CustomEvent('prism_events', {
    detail: {
      type: 'chat',
      data: {
        message: new Array(Math.ceil(Math.random() * 5)).fill(3).map(() => generateTestData())
      }
    }
  }))
}

// close one of the channel during living
const closeYoutubeBtn = appendButton('Living, close youtube channel')
closeYoutubeBtn.onclick = () => {
  dispatchEvent(new CustomEvent("prism_events", {
    detail: {
      type: "platform_close",
      data: {
        platform: 'YOUTUBE'
      }
    }
  }))
}

// close one of the channel during living
const closeTwitchBtn = appendButton('Living, close twitch channel')
closeTwitchBtn.onclick = () => {
  dispatchEvent(new CustomEvent("prism_events", {
    detail: {
      type: "platform_close",
      data: {
        platform: 'TWITCH'
      }
    }
  }))
}

function initSettingPanel() {
  let panel = document.createElement('div')
  panel.style.position = 'absolute'
  panel.style.bottom = '100px'
  panel.style.right = '20px'
  panel.style.padding = '10px 10px 40px'
  panel.style.display = 'none'
  panel.style.backgroundColor = 'rgba(0,0,0,.4)'
  document.body.appendChild(panel)
  return panel
}
function appendButton(text, target) {
  const btn = document.createElement('button')
  btn.style.display = 'block'
  btn.style.margin = '10px'
  btn.innerHTML = text
  btn.style.cursor = 'pointer'
  btn.style.backgroundColor = '#ffffff'
  btn.style.padding = '8px 10px'
  if (!target) target = panel
  target.appendChild(btn)
  return btn
}
function generateTestData() {
  const isOwner = [true, false][Math.floor(Math.random() * 2)]
  const channels = Object.keys(CHANNEL)
  const platform = channels[Math.floor(Math.random() * channels.length)]
  return {
    id: Symbol(),
    livePlatform: platform,
    message: 'test, '.repeat(Math.ceil(Math.random() * 30)),
    publishedAt: new Date(),
    rawMessage: platform === CHANNEL.TWITCH ? '' : '{"id": "23124sfATWerq34","message":{}}',
    author: {
      displayName: isOwner ? 'owner' : Math.random().toString(36).slice(-8),
      isChatOwner: isOwner
    }
  }
}
function generateTestSpecifiedData(message, channel) {
  const isOwner = true
  return {
    id: Symbol(),
    livePlatform: channel,
    message: message,
    publishedAt: new Date(),
    rawMessage: '{"id": "23124sfATWerq34"}',
    author: {
      displayName: isOwner ? 'owner' : Math.random().toString(36).slice(-8),
      isChatOwner: isOwner
    }
  }
}
