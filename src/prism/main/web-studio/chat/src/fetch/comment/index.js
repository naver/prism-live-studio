import request from './request'
import { urlParse, urlStringify, setCookie } from './utils'
import queryString from 'querystring'
import EventEmitter from 'eventemitter3'
import hmacsha1 from 'hmacsha1'
import printLog from '@/assets/js/printLog'

const DEFAULT_PAGESIZE = 50
const DEFAULT_TIMING = 3000
let isDev = process.env.NODE_ENV === 'development'

class Comment extends EventEmitter {
  options = {
    URI: '',
    GLOBAL_URI: '',
    HMAC_SALT: ''
  }

  constructor (options) {
    super()
    this.setOption(options)
  }

  setOption (options = {}) {
    this.options = {...this.options, ...options}
  }

  getOption (attrList = []) {
    let result = {}
    attrList.forEach(key => {
      typeof this.options[key] !== 'undefined'
      && (result[key] = this.options[key])
    })
    return result
  }

  addUrlHmacsha (options) {
    let {
      url,
      params = {},
      type = '',
      now = Date.now()
    } = options

    if (!url) {
      throw new Error('addRequestKeys error from no have url')
    }
    let baseUrl = this.options.GLOBAL_URI || this.options.URI
    let hmacUrl = baseUrl + url
    hmacUrl = urlStringify(hmacUrl, params)
    let md = this.hmac(hmacUrl + now)
    url = urlStringify(url, {...params, msgpad: now, md})
    isDev && (baseUrl = '')
    return baseUrl + url
  }

  hmac (stamp, hmac) {
    return hmacsha1(hmac || this.options.HMAC_SALT, stamp)
  }

  getList () {}

  getOlderComment () {}

  sendComment () {}

  repComment () {}

  delComment () {}
}

class Naver extends Comment {
  lastCommentNo = 0// 最新的commentId
  disableNos = []

  constructor (props) {
    super(props)
    this.setOption({
      groupId: '',
      lang: '',
      objectId: '',
      commentNo: '',
      objectUrl: '',
      ticket: '',
      templateId: '',
      userType: '',
      NID_SES: '',
      nid_inf: '',
      NID_AUT: '',
      pageSize: DEFAULT_PAGESIZE,
      ...props
    })
    this.event = new EventEmitter()
  }

  setAuth () {
    // let option = isDev ? {} : {
    //   domain: this.options.GLOBAL_URI || this.options.URI
    // }
    setCookie('NID_SES', this.options.NID_SES)
    setCookie('nid_inf', this.options.nid_inf)
    setCookie('NID_AUT', this.options.NID_AUT)
  }

  //获取评论列表
  getList (data = {}) {
    let {page = 1, pageSize = this.options.pageSize} = data
    page = Math.max(page, 1)
    let requestUrl = this.addUrlHmacsha({
      url: '/prism_pc/cbox/v2_neo_list_advanced_json.json',
      params: {
        page,
        pageSize,
        ...this.getOption([
          'lang', 'objectId', 'groupId', 'templateId', 'ticket', 'userType'
        ])
      }
    })
    return new Promise((resolve, reject) => {
      printLog({message: 'SlideChat: cbox request start'})
      request.get(requestUrl).then(res => {
        if (res.success) {
          let list = res.result.commentList || []
          let commentNo = list[0] ? list[0].commentNo : null
          commentNo > this.lastCommentNo && (this.lastCommentNo = commentNo)
          resolve(res.result)
          printLog({message: 'SlideChat: cbox request success'})
        } else {
          reject(res)
          printLog({message: 'SlideChat: cbox request error'}, 'error', res)
        }
      }, e => {
        printLog({message: 'SlideChat: cbox request error'}, 'error', e)
        reject(e)
      }).catch(reject)
    })
  }

  // 获取指定范围评论,取比commentNo更老的len条
  getOlderComment (commentNo, len = this.options.pageSize) {
    if (!commentNo) throw new Error('rep comment no commentNO')

    let requestUrl = this.addUrlHmacsha({
      url: '/prism_pc/cbox/v2_neo_next_list_json.json',
      params: {
        pageSize: len || this.options.pageSize,
        commentNo,
        ...this.getOption(['groupId', 'lang', 'objectId', 'templateId', 'ticket', 'userType'])
      }
    })
    printLog({message: 'SlideChat: cbox request start'})
    return new Promise((resolve, reject) => {
      request.get(requestUrl).then(res => {
        if (res.success) {
          resolve((res.result && res.result.commentList) || [])
          printLog({message: 'SlideChat: cbox request success'})
        } else {
          reject(res)
          printLog({message: 'SlideChat: cbox request error'}, 'error', res)
        }
      }, e => {
        printLog({message: 'SlideChat: cbox request error'}, 'error', e)
        reject(e)
      }).catch(reject)
    })
  }

  // 发送评论
  sendComment (data) {
    let {commentType = 'txt', contents = '', extension = ''} = data
    if (!contents) return
    let params = {
      commentType,
      contents,
      ...this.getOption(['lang', 'ticket', 'objectId', 'userType', 'groupId', 'templateId', 'objectUrl'])
    }
    let url = this.addUrlHmacsha({
      url: '/prism_pc/cbox/v2_naver_create_json.json'
    })
    return new Promise((resolve, reject) => {
      this.setAuth()
      printLog({message: 'SlideChat: cbox request start'})
      request.post(url, queryString.stringify(params)).then(res => {
        if (res.success) {
          let list = res.result.commentList.filter(item => {
            let isNew = item.commentNo > this.lastCommentNo && !this.disableNos.includes(item.commentNo)
            isNew && this.disableNos.push(item.commentNo)
            return isNew
          })
          // console.log('sendComment list', list.map(item => item.commentNo))
          resolve(list)
          printLog({message: 'SlideChat: cbox request success'})
        } else {
          reject(res)
          printLog({message: 'SlideChat: cbox request error'}, 'error', res)
        }
      }, e => {
        printLog({message: 'SlideChat: cbox request error'}, 'error', e)
        reject(e)
      })
    })

  }

  // 举报评论
  repComment (commentNo) {
    if (!commentNo) throw new Error('rep comment no commentNO')
    let params = {
      ...this.getOption([
        'lang',
        'ticket',
        'objectId',
        'userType',
        'groupId',
        'templateId']),
      commentNo
    }
    let url = this.addUrlHmacsha({
      url: '/prism_pc/cbox/v2_naver_report_json.json'
    })
    return new Promise((resolve, reject) => {
      this.setAuth()
      printLog({message: 'SlideChat: cbox request start'})
      request.post(url, queryString.stringify(params)).then(res => {
        if (res.code === '5001' || res.code === '5002' || res.success) { //5002:自己的评论不能举报,5001:此评论已经举报
          resolve(res)
          printLog({message: 'SlideChat: cbox request success'})
        } else {
          reject(res)
          printLog({message: 'SlideChat: cbox request error'}, 'error', res)
        }
      }, e => {
        printLog({message: 'SlideChat: cbox request error'}, 'error', e)
      }).catch(reject)
    })
  }

  // 删除评论
  delComment (commentNo) {
    if (!commentNo) throw new Error('Del comment params wrong')
    let params = {
      ...this.getOption(['lang', 'ticket', 'objectId', 'templateId', 'userType', 'groupId']),
      commentNo
    }
    let url = this.addUrlHmacsha({
      url: '/prism_pc/cbox/v2_naver_delete_json.json'
    })
    return new Promise((resolve, reject) => {
      this.setAuth()
      printLog({message: 'SlideChat: cbox request start'})
      request.post(url, queryString.stringify(params)).then(res => {
        if (res.success) {
          resolve(res.result)
          printLog({message: 'SlideChat: cbox request success'})
        } else {
          reject(res)
          printLog({message: 'SlideChat: cbox request error'}, 'error', res)
        }
      }, e => {
        printLog({message: 'SlideChat: cbox request error'}, 'error', e)
        reject(e)
      })
    })
  }
}

class Vlive extends Comment {
  lastCommentNo = 0// 最新的commentId
  disableNos = []

  constructor (props) {
    super(props)
    // todo
    this.setOption({
      gcc: '',
      lang: '',
      ticket: '',
      templateId: '',
      objectUrl: '',
      snsCode: '',
      pool: '',
      objectId: '',
      commentNo: '',
      extension: {},
      version: 1,
      pageSize: DEFAULT_PAGESIZE,
      consumerKey: '',
      Authorization: '',
      ...props
    })
    this.event = new EventEmitter()
  }

  getHeader () {
    return this.getOption(['consumerKey', 'Authorization'])
  }

  //获取评论列表
  getList (data = {}) {
    let {page = 1, pageSize = this.options.pageSize} = data
    page = Math.max(page, 1)
    let requestUrl = this.addUrlHmacsha({
      url: '/globalV2/cbox/v2_neo_list_advanced_json.json',
      params: {
        page,
        pageSize,
        ...this.getOption([
          'gcc', 'lang', 'objectId', 'pool', 'templateId', 'ticket', 'version'
        ])
      }
    })
    return new Promise((resolve, reject) => {
      printLog({message: 'SlideChat: cbox request start'})
      request.get(requestUrl, {
        headers: this.getHeader()
      }).then(res => {
        if (res.success) {
          let list = res.result.commentList || []
          let commentNo = list[0] ? list[0].commentNo : null
          commentNo > this.lastCommentNo && (this.lastCommentNo = commentNo)
          resolve(res.result)
          printLog({message: 'SlideChat: cbox request success'})
        } else {
          reject(res)
          printLog({message: 'SlideChat: cbox request error'}, 'error', res)
        }
      }, e => {
        printLog({message: 'SlideChat: cbox request error'}, 'error', e)
        reject(e)
      })
    })
  }

  // 获取指定范围评论,取比commentNo更老的len条
  getOlderComment (commentNo, len = this.options.pageSize) {
    if (!commentNo) throw new Error('rep comment no commentNO')

    let requestUrl = this.addUrlHmacsha({
      url: '/globalV2/cbox4/v2_neo_next_list_json.json',
      params: {
        pageSize: len || this.options.pageSize,
        commentNo,
        ...this.getOption(['gcc', 'lang', 'objectId', 'pool', 'templateId', 'ticket', 'version'])
      }
    })
    return new Promise((resolve, reject) => {
      printLog({message: 'SlideChat: cbox request start'})
      request.get(requestUrl, {
        headers: this.getHeader()
      }).then(res => {
        if (res.success) {
          resolve((res.result && res.result.commentList) || [])
          printLog({message: 'SlideChat: cbox request success'})
        } else {
          printLog({message: 'SlideChat: cbox request error'}, 'error', res)
          reject(res)
        }
      }, e => {
        printLog({message: 'SlideChat: cbox request error'}, 'error', e)
        reject(e)
      })
    })
  }

  // 发送评论
  sendComment (data) {
    let {commentType = 'txt', contents = '', extension = this.options.extension} = data
    if (!contents) return

    typeof extension === 'object' && (extension = JSON.stringify(extension))

    let params = {
      commentType,
      contents,
      extension: '%7B%7D',
      ...this.getOption(['lang', 'ticket', 'objectId', 'snsCode', 'objectUrl', 'templateId', 'userProfileImage', 'userName', 'profileId'])
    }
    let url = this.addUrlHmacsha({
      url: '/globalV2/cbox4/v2_neo_create_json.json?pool=' + this.options.pool
    })
    return new Promise((resolve, reject) => {
      printLog({message: 'SlideChat: cbox request start'})
      request.post(url, queryString.stringify(params), {
        headers: this.getHeader()
      }).then(res => {
        if (res.success) {
          let list = res.result.commentList.filter(item => {
            let isNew = item.commentNo > this.lastCommentNo && !this.disableNos.includes(item.commentNo)
            isNew && this.disableNos.push(item.commentNo)
            return isNew
          })
          // console.log('sendComment list', list.map(item => item.commentNo))
          resolve(list)
          printLog({message: 'SlideChat: cbox request success'})
        } else {
          printLog({message: 'SlideChat: cbox request error', data: res})
          reject(res)
        }
      }, e => {
        printLog({message: 'SlideChat: cbox request error', data: e}, 'error')
        reject(e)
      })
    })

  }

  // 举报评论
  repComment (commentNo) {
    if (!commentNo) throw new Error('rep comment no commentNO')
    let params = {
      commentNo,
      ...this.getOption([
        'lang',
        'ticket',
        'objectId',
        'templateId'])
    }
    let url = this.addUrlHmacsha({
      url: '/globalV2/cbox4/v2_neo_report_json.json?pool=' + this.options.pool
    })
    return new Promise((resolve, reject) => {
      printLog({message: 'SlideChat: cbox request start'})
      request.post(url, queryString.stringify(params), {
        headers: this.getHeader()
      }).then(res => {
        let err = res.code === '5002' ? new Error('自己的评论不能删除') : res
        if (res.success) {
          resolve(res.result)
          printLog({message: 'SlideChat: cbox request success'})
        } else {
          reject(err)
          printLog({message: 'SlideChat: cbox request error', data: res})
        }
      }, e => {
        printLog({message: 'SlideChat: cbox request error', data: e}, 'error')
        reject(e)
      })
    })
  }

  // 删除评论
  delComment (commentNo) {
    if (!commentNo) throw new Error('Del comment params wrong')
    let params = {
      ...this.getOption(['lang', 'ticket', 'objectId', 'templateId']),
      commentNo
    }
    let url = this.addUrlHmacsha({
      url: '/globalV2/cbox4/v2_neo_delete_json.json?pool=' + this.options.pool
    })
    return new Promise((resolve, reject) => {
      printLog({message: 'SlideChat: cbox request start'})
      request.post(url, queryString.stringify(params), {
        headers: this.getHeader()
      }).then(res => {
        if (res.success) {
          resolve(res.result)
          printLog({message: 'SlideChat: cbox request success'})
        } else {
          reject(res)
          printLog({message: 'SlideChat: cbox request error', data: res})
        }
      }, e => {
        printLog({message: 'SlideChat: cbox request error', data: e}, 'error')
        reject(e)
      })
    })
  }

}

class CommentManage extends EventEmitter {
  request = null
  eventNames = ['NEW_COMMENTS']
  timer = 0
  pollTime = DEFAULT_TIMING // 轮训间隔ms
  isPolling = false
  storageBar = {
    total: 0,
    index: null,
    commentNo: null,
    totalPages: null
  }

  //props type, objectId
  constructor (props) {
    super(props)
    if (!props.objectId) throw new Error('objectId is not empty')
    this.pollTime = props.pollTime || this.pollTime
    if (props.type === 'VLIVE') {
      this.request = new Vlive(props)
    } else if (props.type === 'NAVERTV') {
      this.request = new Naver(props)
    } else {
      throw  new Error('type is not support')
    }
  }

  setOption (options = {}) {
    this.request.setOption(options)
  }

  async start () {
    this.stop()
    this.isPolling = true
    return this.polling(true)
  }

  stop () {
    this.isPolling = false
    this.request.disableNos = []
    clearTimeout(this.timer)
  }

  dispatchNewList (list = [], type = 'last') {
    if (!list.length) return
    list = list.filter(item => {
      return item.commentNo > this.storageBar.commentNo && !this.request.disableNos.includes(item.commentNo)
    })
    if (list.length) {
      this.storageBar.commentNo = list[0].commentNo
      // console.log('dispatch list', list.map(item => item.commentNo))
      this.emit('NEW_COMMENTS', {type, list})
    }
  }

  async updateComments (pageInfo) {
    let type = 'next'
    let {pageModel, commentList} = await this.request.getList({
      page: pageInfo.totalPages - this.storageBar.index
    })
    if (pageInfo.totalPages !== pageModel.totalPages) {
      return this.updateComments(pageModel)
    } else {
      this.dispatchNewList(commentList, 'next')
      this.storageBar.totalPages = pageModel.totalPages
      this.storageBar.index++
      return commentList
    }
  }

  async polling () {
    try {
      let {pageModel, commentList = []} = await this.request.getList()
      if (!this.storageBar.commentNo) {
        this.storageBar.index = pageModel.totalPages - pageModel.page
        this.storageBar.totalPages = pageModel.totalPages
        this.dispatchNewList(commentList, 'last')
      } else if (pageModel.totalPages === this.storageBar.totalPages) {
        this.dispatchNewList(commentList, 'next')
      } else if (commentList.length && commentList[0].commentNo !== this.storageBar.commentNo) {
        commentList = await this.updateComments(pageModel)
      }
    } catch (e) {
      setTimeout(() => {
        this.polling()
      }, DEFAULT_TIMING)
      return
    }
    clearTimeout(this.timer)
    this.isPolling && (this.timer = setTimeout(() => {
      return this.polling()
    }, this.pollTime))
  }
}

/*
* 参数配置说明*/
const LIVE_OPTION_CONFIG = {
  //VLIVE AND NAVERTV
  objectId: '',
  lang: '',
  ticket: '',
  objectUrl: '',
  templateId: '',
  URI: 'http://dev.apis.naver.com',
  GLOBAL_URI: 'http://dev-global.apis.naver.com',
  HMAC_SALT: 'M1lTEG3HZUPurwXdJVDB1YQnTwp3PepE3cv8HKED4KARZDlowqlbG0aZRr6l2e22',

  // NAVERTV
  userType: '',
  groupId: '',
  NID_SES: '',
  nid_inf: '',
  NID_AUT: '',

  // VLIVE option
  gcc: '',
  snsCode: '',
  pool: '',
  commentNo: '',
  extension: {},
  version: 1,
  consumerKey: '',
  Authorization: ''
}

async function testApi () {
  let data
  let VLIVE_TESE_OPTIONS = {
    gcc: 'KR',
    lang: 'ko',
    ticket: 'globalv',
    templateId: 'default',
    objectUrl: 'http://www.vlive.tv',
    snsCode: 'naver',
    pool: 'cbox4',
    objectId: '144488',
    extension: {
      'no': '4436',
      'lv': '0',
      'cno': 189
    },
    version: 1,
    URI: 'http://dev.apis.naver.com',
    GLOBAL_URI: 'http://dev-global.apis.naver.com',
    HMAC_SALT: 'y4nRKGR9QhCnP10vCJ4tvfUUHYpOMq9N8WRmWWTA9DHBVkd9t3xaRCUdeyYW5I3z',
    consumerKey: 'EvbbxIFhoa3Z2SkNwe0K',
    Authorization: 'Bearer uSLbDyC+O6avycceuS2Jfi1KpfMvkDlNXccPx+DUOm0/No4w41cVl16p/aO8WLsi9l0sO/Sbd469ARL3iEFwxH69lc6AwcBncCjwzeKc1txTV2f3Z/Db+pMjifsVLNfxlxSdpKX6FqpHqNHqAeL127+UjHzcqw3wdbAPRw0qQLSJ7sa+80pAlwJlZTXZCCLe/XXVGnBqSg/gDeG6eTLR/0HhQ8QBDqcheb9g3lnXcwwLDnn13o5TEEAOxRiLO+6QrvXtZ7hGJkxRZ33LmlIQ1M3imsi0HDRKzJqNz5OB8yDkFQXM5BDgMxChhqzwlF13qmllHe7GVb+PHC1ncXBGPpFTXxOJN/TESAthm+fFrl0Ae0H0nhPqeERiddhjha0tnu0fucjrdClU0mZLxVVzXEVVDfvlrvmB1VQTEMoDj2FHlMs3pA7cuHxcMGQUKXvp'
  }
  let NAVERTV_TESE_OPTIONS = {
    objectId: 'CH_89284483',
    groupId: 'glivetest3',
    lang: 'ko',
    objectUrl: 'http://tv.naver.com/',
    ticket: 'tvcastlive',
    templateId: 'talk_app',
    userType: 'MANAGER',
    URI: 'http://dev.apis.naver.com',
    GLOBAL_URI: 'http://dev-global.apis.naver.com',
    HMAC_SALT: 'M1lTEG3HZUPurwXdJVDB1YQnTwp3PepE3cv8HKED4KARZDlowqlbG0aZRr6l2e22',
    NID_SES: 'AAABpPlBqsG6A6GNtLX5MmlP/Y2K8HIvmEjYNRut18Hr7bhz70QTRgc/b9i0CxphLel4TcijLcs6q/477RUeQghjI1yrRk5243xCJNsNuaWFBv1suG/MYpx5fRA7yvHTgKIDtOUnI0d3lam7ulSm0jUrWHIZWQOQRo8kP2qkj5jvF9SQ/q2PiqQw6TRqk29ij1WJjvQlMSlBdru7fu2gUQyQOoYbgh1iYHQoFhGYnOzcsfumIHm4KcG6VjRn2xLcyucPeocYXysyM/KOmDmlsVh9A1BrSG/YeUU3zNw4p3ABmK6uLsPe6NaZgtKX6zIOW8KEF1J3ojN63VbbO1/5aWqabl8ZV35Dvxiw6zOk7U5JMCo1mkcLVWQLCSARgyiLyQhg39xw5So0tGWtn3OsnzllGkRJoBwTC02/PYbg6IfHDnM0e3TX5KpoQG5A35ZXepHt8f6ehi2OTjuDcgcaugIoto0eG7sd9Z5ruyx/GUGXAdvCjzj2MkwI+VOhmKRbqG0HPJ/QBi6ANpvHebH3ppp13C8agpol8exAkETiWge4aPiDTmfM0HgyriX/YD+IzrmopQ==',
    nid_inf: '-1162647410',
    NID_AUT: '3LjPEn2v1akrXj1Ey5udni3tsiRQaavKYcFqdukdLZJND1yRl1ouiSFedGHUK4Jf'
  }
  const comment = new CommentManage({
    type: 'NAVERTV',
    pollTime: 3000,
    pageSize: 50,
    ...NAVERTV_TESE_OPTIONS
  })

  // console.log(comment.request.hmac('',''))

  comment.on('NEW_COMMENTS', newComments => {
    console.log('newComments', newComments.type, newComments.list.map(item => item.commentNo))
  })

  // comment.start()

  data = await comment.request.sendComment({
    commentType: 'txt',
    contents: 'chao' + Date.now()
  })
  console.log('send comment', data)

  data = await comment.request.getList({page: 0, pageSize: 2})
  console.log('comment list', data)
  //
  // // comment.stop()
  //
  // data = await comment.request.getOlderComment(321529,1)
  // console.log('older list', data)
  //
  // data = await comment.request.delComment(321526)
  // console.log('del comment', data)
  //
  // data = await comment.request.repComment(321528)
  // console.log('rep comment', data)
}

export {
  testApi,
  CommentManage
}
