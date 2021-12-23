import { handleJson } from '@/assets/js/utils'
import printLog from '@/assets/js/printLog'
import request from '@/fetch/comment/request.js'
import jsCookie from 'js-cookie'
import io from './socket.io-2.0.3'
import { urlParse, urlStringify } from '../../fetch/comment/utils'
import queryString from 'querystring'
import hmacsha1 from 'hmacsha1'

let isDev = process.env.NODE_ENV === 'development'
const reconnectTime = 5000 //5s重连
export default class ShoppingLiveChat {
  socket = null
  reconnectTimer = 0
  reconnectCout = 3
  options = {
    hmac: '',
    broadcastId: '',
    host: '',
    'X-LIVE-COMMERCE-AUTH': '',
    'X-LIVE-COMMERCE-DEVICE-ID': '',
    handler () {}
  }

  constructor (options) {
    if (!options) return printLog({message: 'SHOPPINGLIVEChat: Missing parameters for websocket chat connection'})
    if (!options.broadcastId) printLog({message: 'SHOPPINGLIVEChat: Missing broadcastId for websocket chat connection'})
    this.reconnectCout = 3
    this.setOptions(options)
    this.connectChatSocket()
  }

  setOptions (data) {
    this.options = Object.assign(this.options, data)
  }

  getHeaders () {
    return {
      'X-LIVE-COMMERCE-AUTH': this.options['X-LIVE-COMMERCE-AUTH'],
      'X-LIVE-COMMERCE-DEVICE-ID': this.options['X-LIVE-COMMERCE-DEVICE-ID']
    }
  }

  addUrlHmacsha (options) {
    let {
      url,
      params = {},
      now = Date.now()
    } = options

    if (!url) {
      throw new Error('addRequestKeys error from no have url')
    }
    let baseUrl = this.options.host
    let hmacUrl = baseUrl + url
    hmacUrl = urlStringify(hmacUrl, params)
    let md = hmac(hmacUrl + now, this.options.hmac)
    url = urlStringify(url, {...params, msgpad: now, md})
    isDev && (baseUrl = '/shopping')
    window.hmac = hmac
    return baseUrl + url

    function urlStringify (url, data = {}) {
      let params = urlParse(url)
      let params2 = queryString.stringify({...params, ...data})
      return (url.split('?').shift() || '') + (params2 ? '?' + params2 : '')
    }

    function hmac (stamp, hmac) {
      return hmacsha1(hmac, stamp)
    }
  }

  async connectChatSocket () {
    let requestUrl = this.addUrlHmacsha({
      url: `/prism_pc/broadcast_api_app/v1/broadcast/${this.options.broadcastId}/auth`,
      params: {}
    })
    let res = null
    printLog({message: 'SHOPPINGLIVEChat: Start request websocket address'})
    try {
      res = await request.get(requestUrl, {
        headers: this.getHeaders()
      })
      printLog({message: 'SHOPPINGLIVEChat: The websocket connection address is successfully obtained'})
    } catch (e) {
      printLog({message: 'SHOPPINGLIVEChat: Failed to obtain websocket connection address', data: e})
    }
    let socketIoOption = {
      'reconnection': true,
      'force new connection': true,
      'connect timeout': 3000,
      'transports': ['websocket'] // IE8,9는 polling 을 사용해야 한다.
    }
    this.socket = io.connect(res.url, socketIoOption)
    this.socket.on('connect', () => {
      printLog({message: 'SHOPPINGLIVEChat: websocket chat connection is successful'})
      this.options.handler({
        type: 'broadcast_start'
      })
      clearTimeout(this.reconnectTimer)
    })
    this.socket.on('error', data => {
      printLog({message: 'SHOPPINGLIVEChat: websocket chat connection error', data: {data, res}}, 'error')
      this.reConnect()
    })
    this.socket.on('broadcast_chat', (data) => {
      this.options.handler({
        type: 'broadcast_chat', data: JSON.parse(data)
      })
    })
    this.socket.on('broadcast_fixed_notice', (data) => {
      this.options.handler({
        type: 'broadcast_fixed_notice', data: JSON.parse(data)
      })
    })
    this.socket.on('broadcast_notice', (data) => {
      this.options.handler({
        type: 'broadcast_notice', data: JSON.parse(data)
      })
    })
  }

  reConnect () {
    if (this.reconnectCout > 0) {
      clearTimeout(this.reconnectTimer)
      this.reconnectTimer = setTimeout(() => {
        printLog({message: 'SHOPPINGLIVEChat: websocket chat connection reconnect', data: 'How many times to reconnect:' + (4 - this.reconnectCout)})
        this.reconnectCout = this.reconnectCout - 1
        this.socket && this.socket.close()
        this.connectChatSocket()
      }, reconnectTime)
    } else {
      printLog({message: 'SHOPPINGLIVEChat: All reconnection failed, unable to chat'}, 'error')
    }
  }

  close () {
    printLog({message: 'SHOPPINGLIVEChat: websocket chat connection is closed'})
    this.socket && this.socket.close && this.socket.close()
  }

  createNotice (item, isResend) {
    let requestUrl = this.addUrlHmacsha({
      url: `/prism_pc/broadcast_api_app/v1/notice`,
      params: {}
    })
    let {noticeType = 'CHAT', broadcastId, message} = item
    let chatData = {
      broadcastId: this.options.broadcastId,
      noticeType,
      message
    }
    return request.post(requestUrl, chatData, {
      headers: this.getHeaders()
    })
  }

  deleteNotice (item) {
    let {broadcastId, id} = item
    let params = {
      id, broadcastId: this.options.broadcastId
    }
    let requestUrl = this.addUrlHmacsha({
      url: `/prism_pc/broadcast_api_app/v1/notice/${id}`,
      params
    })
    return request.delete(requestUrl, {
      headers: this.getHeaders()
    })
  }
}
