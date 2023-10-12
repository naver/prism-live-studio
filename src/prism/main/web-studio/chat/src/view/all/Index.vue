<template>
  <chats-wrapper
      :is-all-channel="true"
      :chat="chatData"
      :show-chats="isLiving"
      :no-live-tips="$i18n.noLive"
      :disable-input="!isLiving || isClosedDuringLiving"
      @live-init="handleLiveInit"
      @live-end="handleLiveEnd"
      @live-platform-close="handlePlatformClose"
      @token-refresh="handleTokenReset"
      @chat-receive="handleChat"
      @handle-cached-exceed="handleCachedExceed"
      @re-broadcast="handleReBroadcast"
      @handle-send="handleSend">
    <template v-slot:chat="{ item }">
      <template-afreeca v-if="isAfreecaTV(item.livePlatform)" :data="item"></template-afreeca>
      <template-normal v-else :data="item" @handle-delete="handleDelete"></template-normal>
    </template>
  </chats-wrapper>
</template>

<script>
import { MAX_CACHED_CHAT_COUNT, MAX_LOCAL_CACHED_CHAT_COUNT, CHANNEL_ON_ALL_PAGE, CHANNEL } from '@/assets/js/constants'
import { CommentManage } from '@/fetch/comment'
import { handleJson } from '@/assets/js/utils'
import { getTwitchResources } from '@/assets/js/twitchResources'
import timeago from '@/assets/js/timeago'
import AfreecaTV from '@/components/components/afreecaTV'
import ChatsWrapper from '@/components/components/ChatsWrapper.vue'
import TemplateNormal from '@/components/template/Normal.vue'
import TemplateAfreeca from '@/components/template/AfreecaTV.vue'
import api from '@/fetch/youtube'
import printLog from '@/assets/js/printLog'

export default {
  name: 'AllChat',
  mixins: [AfreecaTV],
  components: {
    ChatsWrapper, TemplateNormal, TemplateAfreeca
  },
  props: {},
  data () {
    return {
      isLiving: false,
      chatData: [],
      prismParams: {},
      tokenDeniedPool: {},
      inTokenRefreshing: {},
      commentManager: null,
      closedPlatforms: [],
      reportedIDs: []
    }
  },
  watch: {
    chatData (now) {
      this.isLiving && localStorage.setItem('allChats', JSON.stringify({
        videoSeq: this.prismParams.videoSeq,
        list: now.slice(0, MAX_LOCAL_CACHED_CHAT_COUNT) //最多本地缓存200条聊天信息
      }))
    }
  },
  computed: {
    isClosedDuringLiving () {
      return this.isLiving && this.isAllPlatformClosed(this.closedPlatforms, this.prismParams.platforms.map(el => el.name))
    },
    userId_navertv () {
      const navertv = this.isLiving && this.getPlatformInfo(CHANNEL.NAVERTV)
      return navertv && navertv.userId
    },
    userId_afreecatv () {
      const afreecatv = this.isLiving && this.getPlatformInfo(CHANNEL.AFREECATV)
      return afreecatv && afreecatv.userId && afreecatv.userId.replace(/\([\s\S]*\)/g, '')
    },
    twitchClientId () {
      const twitch = this.isLiving && this.getPlatformInfo(CHANNEL.TWITCH)
      return twitch && twitch.clientId
    }
  },
  methods: {
    handleLiveInit (data) {
      if (this.isLiving) return
      const {platforms, ...params} = data
      this.prismParams = {...params}
      if (platforms) {
        this.prismParams.platforms = platforms.filter(el => CHANNEL_ON_ALL_PAGE.includes(el.name))
        printLog({message: 'ALLChat: Get the initialization data'})
        if (this.prismParams.platforms.length) this.isLiving = true
      }
      if (this.prismParams.videoSeq) {
        let storageList = this.readStorage(this.prismParams.videoSeq)
        storageList && (this.chatData = storageList)
      }
    },
    readStorage (videoSeq) {
      printLog({message: `ALLChat: read chat storage`})
      let storage = localStorage.getItem('allChats') || null
      typeof storage === 'string' && (storage = JSON.parse(storage))
      localStorage.removeItem('allChats')
      if (!storage || storage.videoSeq !== videoSeq) {
        printLog({message: `ALLChat: read chat storage but no chat data`})
        return false
      }
      printLog({message: `ALLChat: read chat storage success, storage chat length${storage.list.length}`})
      storage.list.forEach(i => i.id = Symbol())
      return storage.list
    },
    handleLiveEnd (data) {
      printLog({message: 'ALLChat: The live broadcast is over'})
      this.isLiving = false
      this.chatData = []
      this.prismParams = {}
      this.tokenDeniedPool = {}
      this.inTokenRefreshing = {}
      this.closedPlatforms = []
      this.reportedIDs = []
      this.clearCommentManager()
    },
    handlePlatformClose (data) {
      if (!this.isLiving) return
      if (!this.prismParams.platforms.map(el => el.name).includes(data.platform)) return
      this.closedPlatforms.push(data.platform)
      printLog({message: 'ALLChat: This chat platform is disabled'})
    },
    handleTokenReset (data) {
      if (!this.isLiving) return
      this.handleTokenDeniedCallback(data.platform, data.token)
    },
    handleTokenDeniedCallback (platform, accessToken) {
      let platformInfo = this.getPlatformInfo(platform)
      if (!platformInfo) {
        printLog({message: 'ALLChat: Reset Token but no have platformInfo'})
        return
      }
      platformInfo.accessToken = accessToken
      this.inTokenRefreshing[platform] = false
      let tokenDeniedPool = this.tokenDeniedPool
      let deletePool = tokenDeniedPool[platform]
      if (!deletePool) return
      for (let id in deletePool) {
        if (!deletePool[id].completed) {
          this.handleDeleteRequest(deletePool[id].data)
          deletePool[id].completed = true
        }
      }
    },
    handleSend (data) {
      printLog({message: 'ALLChat: send chat'})
      window.sendToPrism(JSON.stringify({
        type: 'send',
        data: {
          message: data
        }
      }))
    },
    handleDelete (data) {
      printLog({message: 'ALLChat: delete chat'})
      const {notice, confirm: {deleteText, deleteConfirmButton, cancelButton}} = this.$i18n
      this.$confirm({
        title: notice,
        content: deleteText,
        confirmText: deleteConfirmButton,
        cancelText: cancelButton
      }).then(() => {
        this.handleDeleteRequest(data)
      })
    },
    handleDeleteRequest (data) {
      const rawMessage = data.rawMessage
      const livePlatform = data.livePlatform

      let platformInfo = this.getPlatformInfo(livePlatform)
      if (!platformInfo) {
        return
      }
      switch (livePlatform) {
        case CHANNEL.YOUTUBE:
          const id = rawMessage.id
          if (!this.tokenDeniedPool[livePlatform]) {
            this.tokenDeniedPool[livePlatform] = {}
          }
          let deletePool = this.tokenDeniedPool[livePlatform]
          if (this.inTokenRefreshing[livePlatform]) {
            if (!deletePool[id]) {
              deletePool[id] = {completed: false, data}
            }
            return
          }

          api.delete({
            id,
            accessToken: platformInfo.accessToken
          }).then(res => {
            if (res.status === 204) {
              const index = this.chatData.indexOf(data)
              index !== -1 && this.chatData.splice(index, 1)
            }
          }).catch((err) => {
            if (err.response && err.response.status === 401) {
              if (!deletePool[id]) {
                deletePool[id] = {completed: false, data}
              }
              this.requestToken(livePlatform)
            }
          })
          break
        case CHANNEL.NAVERTV:
          const commentNo = rawMessage.commentNo
          this.handleCommentManagerInit(platformInfo)
          printLog({message: 'ALLChat: delete chat request start'})
          this.commentManager.request.delComment(commentNo).then(() => {
            const index = this.chatData.indexOf(data)
            index !== -1 && this.chatData.splice(index, 1)

            window.sendToPrism(JSON.stringify({
              type: 'broadcast',
              data: {
                receive: CHANNEL.NAVERTV,
                type: 'delete',
                data: commentNo
              }
            }))
            printLog({message: 'ALLChat: delete chat request success'})
          }).catch(e => {
            printLog({message: 'ALLChat: delete chat request error', data: e}, 'error')
          })
          break
      }
    },
    handleChat (data) {
      if (!this.isLiving) return
      const res = []
      data.forEach(el => {
        el.id = Symbol()
        el.timeago = timeago(el.publishedAt, this.$lang)
        try {
          el.rawMessage = handleJson(el.rawMessage)
        } catch {}
        el._isSelf = el.author.isChatOwner

        switch (el.livePlatform) {
          case CHANNEL.NAVERTV:
            if (!this.reportedIDs.includes(el.rawMessage.userIdNo)) {
              el._isSelf = el.rawMessage.userIdNo === this.userId_navertv
              el._isManager = !el._isSelf && el.rawMessage.manager
              res.push(el)
            }
            break
          case CHANNEL.AFREECATV:
            let handledData = this.handleAfreecatvChat(el, this.userId_afreecatv)
            handledData && res.push(handledData)
            break
          case CHANNEL.TWITCH: {
            let raws = {}
            el.rawMessage.split(';').forEach(raw => {
              const [key, value] = raw.split('=')
              raws[key] = value
            })
            el.rawMessage = raws
            raws['room-id'] && getTwitchResources({
              channelId: raws['room-id'],
              clientId: this.twitchClientId
            })
            res.push(el)
            break
          }
          case CHANNEL.FACEBOOK:
            let attachment = el.rawMessage.attachment || {}
            if (el.message) {
              res.push(el)
            } else if (attachment.type === 'animated_image_share' && attachment.media.source) {
              res.push(el)
            } else if (attachment.type === 'sticker') {
              res.push(el)
            }
            break
          default:
            res.push(el)
            break
        }
      })
      this.chatData.push(...res)
      res.length && printLog({message: 'ALLChat: Receive new chat'})
    },
    requestToken (platform) {
      if (!this.inTokenRefreshing[platform]) {
        printLog({message: 'ALLChat: Request token information like prism again'})
        this.inTokenRefreshing[platform] = true
        window.sendToPrism(JSON.stringify({
          type: 'token',
          data: {
            platform
          }
        }))
      }
    },
    handleCommentManagerInit (platformInfo) {
      if (!this.commentManager) {
        const {name, ...params} = platformInfo
        this.commentManager = new CommentManage({
          type: name,
          ...params
        })
      }
    },
    clearCommentManager () {
      if (this.commentManager) {
        this.commentManager = null
      }
    },
    handleCachedExceed () {
      const length = this.chatData.length
      if (length <= MAX_CACHED_CHAT_COUNT) return
      const count = length - MAX_CACHED_CHAT_COUNT + MAX_CACHED_CHAT_COUNT * 0.1
      this.chatData.splice(0, count)
    },
    handleReBroadcast (data) {
      data = handleJson(data)
      if (data.receive === 'ALL') {
        switch (data.type) {
          case 'report':
            this.reportedIDs.push(data.data)
            const filtered = this.chatData.filter(el => el.rawMessage.userIdNo !== data.data)
            this.chatData.length = 0
            this.chatData.push(...filtered)
            break
          case 'delete':
            const index = this.chatData.findIndex(el => {
              return el.rawMessage.commentNo === data.data
            })
            index !== -1 && this.chatData.splice(index, 1)
            break
        }
      }
    },
    getPlatformInfo (ptName) {
      return this.prismParams.platforms && this.prismParams.platforms.find(el => el.name === ptName)
    },
    isAllPlatformClosed (closed, platforms) {
      if (!closed || !platforms) return false
      let closed_ = [...new Set(closed)].filter(el => el !== CHANNEL.FACEBOOK)
      let platforms_ = [...new Set(platforms)].filter(el => el !== CHANNEL.FACEBOOK)
      if (closed_.length !== platforms_.length) return false
      for (let i = 0; i < closed_.length; i++) {
        if (!platforms_.includes(closed_[i])) {
          return false
        }
      }
      return true
    },
    isAfreecaTV (platform) {
      return platform === CHANNEL.AFREECATV
    }
  },
  beforeDestroy () {
    printLog({message: 'ALLChat: The chat page is closed'})
    this.clearCommentManager()
  },
  mounted () {
    printLog({message: 'ALLChat: The chat page loads successfully'})
  }
}
</script>
