import { CHANNEL } from '@/assets/js/constants'


let platform = new URLSearchParams(document.location.search).get('platform') || ''
platform = platform.toLocaleUpperCase()

window.sendToPrism = function() {}

let panel = initSettingPanel()

const settingBtn = appendButton('settings', document.body)
settingBtn.style.position = 'absolute'
settingBtn.style.bottom = '80px'
settingBtn.style.right = '80px'
settingBtn.onclick = () => {
  panel.style.display = panel.style.display === 'block' ? 'none' : 'block'
}


// start living - naverTV
if (platform === CHANNEL.NAVERTV) {
  const initBtn = appendButton('Go Live(naverTV)')
  initBtn.onclick = () => {
    window.dispatchEvent(new CustomEvent("prism_events", {
      detail: {
        type: "init",
        data: {
          platforms: [
            {
              lang: '',
              objectUrl: '',
              groupId: '',
              ticket: '',
              userId: '',
              username: 'dfdfg',
              templateId: '',
              userType: '',
              URI: '',
              GLOABL_URI: '',
              HMAC_SALT: '',
              objectId: '',
              NID_SES: '',
              nid_inf: '',
              NID_AUT: ''
            }
          ]
        }
      }
    }))
  }
}

// start living - vlive
if (platform === CHANNEL.VLIVE) {
  const initVliveBtn = appendButton('Go Live(vlive)')
  initVliveBtn.onclick = () => {
    window.dispatchEvent(new CustomEvent("prism_events", {
      detail: {
        type: "init",
        data: {
          platforms: [
            {
              gcc: '',
              lang: '',
              ticket: '',
              templateId: '',
              objectUrl: '',
              snsCode: '',
              pool: '',
              version: 1,
              URI: '',
              GLOABL_URI: '',
              HMAC_SALT: '',
              extension: {
                no: '',
                lv: '',
                cno: 0
              },
              isVliveFanship: false,
              username: 'sfsdfsdf',
              userProfileImage: '',
              objectId: '',
              consumerKey: '',
              Authorization: ''
            }
          ]
        }
      }
    }))
  }
}

if (platform === CHANNEL.FACEBOOK) {
  const initVliveBtn = appendButton('Go Live(facebook)')
  initVliveBtn.onclick = () => {
    window.dispatchEvent(new CustomEvent("prism_events", {
      detail: {
        type: "init",
        data: {
          platforms: [
            {
              name: 'FACEBOOK',
              isPrivate: false
            }
          ]
        }
      }
    }))
  }
}

if (platform === CHANNEL.AFREECATV) {
  const initVliveBtn = appendButton('Go Live(afreecaTV)')
  initVliveBtn.onclick = () => {
    window.dispatchEvent(new CustomEvent("prism_events", {
      detail: {
        type: "init",
        data: {
          platforms: [
            {
              name: 'AFREECATV',
              userId: 'dfsdfs'
            }
          ]
        }
      }
    }))
  }
}


// finish living
const endBtn = appendButton('Finish Live')
endBtn.onclick = () => {
  dispatchEvent(new CustomEvent("prism_events", {
    detail: {
      type: "end"
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

export function appendButton(text, target) {
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
