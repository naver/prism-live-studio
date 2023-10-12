<template>
  <chats-wrapper
      ref="chatsWrapper"
      key-field="commentNo"
      time-field="modTimeGmt"
      message-field="contents"
      :chat="renderData"
      :no-live-tips="$i18n.noLiveNaver.replace('${platform}', $i18n.shoppingLive.name)"
      :show-chats="showChats"
      :disable-input="disableInput"
      :max-input-character="400"
      :fixedMessage="fixedMessage"
      @live-init="handleLiveInit"
      @live-end="handleLiveEnd"
      @chat-receive="chatReceive"
      @one-hour-limit="handleHourLimit"
      @live-platform-close="handlePlatformClose"
      @handle-delete="handleDelete"
      @handle-delete-fixed="handleDeleteFixed"
      @retry-fixed-notice="resendFixedNotice"
      @change-limit="changeLimit"
      @handle-send="handleSend">
    <template v-slot:chat="{ item }">
      <Shopping
          :disable-btn="actionLimit"
          :data="item"
          @handle-delete="handleDelete"
          @handle-retry="handleRetry"
          @show-operation-buttons="item._isShowErrorHandleBtns = true">
      </Shopping>
    </template>
  </chats-wrapper>
</template>

<script>
import {
  CHANNEL,
  CHANNEL_NAME,
  MAX_CACHED_CHAT_COUNT,
  MAX_LOCAL_CACHED_CHAT_COUNT
} from '@/assets/js/constants'
import timeago from '@/assets/js/timeago'
import { handleJson } from '@/assets/js/utils'
import ChatsWrapper from './Wrapper'
import Shopping from '@/components/template/Shopping.vue'
import printLog from '@/assets/js/printLog'
import ShoppingLiveChat from './apis'

export default {
  name: 'prismNaver',
  components: {
    ChatsWrapper, Shopping
  },
  data () {
    return {
      channel: 'navershopping',
      chatData: [],
      isLiving: false,
      showChats: false,
      prismParams: {},
      sendErrorList: [],
      isPlatformClose: false,
      fixedMessage: null,
      fixedErrorMessage: null,
      shoppingApi: null,
      actionLimit: false
    }
  },
  watch: {
    chatData (now) {
      this.setStorage()
    },
    fixedMessage (now) {
      this.setStorage()
    }
  },
  computed: {
    renderData () {
      return this.chatData.concat(this.sendErrorList)
    },
    disableInput () {
      return !this.showChats || this.isPlatformClose
    }
  },
  methods: {
    setStorage () {
      this.isLiving && localStorage.setItem('shoppingChats', JSON.stringify({
        videoSeq: this.prismParams.videoSeq,
        list: this.chatData.slice(0, MAX_LOCAL_CACHED_CHAT_COUNT),
        fixedMessage: this.fixedMessage
      }))
    },
    changeLimit (now) {
      this.actionLimit = now
    },
    readStorage (videoSeq) {
      printLog({message: `SHOPPINGLIVEChat: start read chat storage`})
      let storage = localStorage.getItem('shoppingChats') || null
      typeof storage === 'string' && (storage = JSON.parse(storage))
      localStorage.removeItem('shoppingChats')
      if (!storage || storage.videoSeq !== videoSeq) {
        printLog({message: `SHOPPINGLIVEChat: read chat storage but no chat data`})
        return false
      }
      printLog({message: `SHOPPINGLIVEChat: read chat storage success, storage chat length${storage.list.length}`})
      return storage
    },
    handleLiveInit (data) {
      if (!data) return
      printLog({message: 'SHOPPINGLIVEChat: Get the initialization data'})
      process.env.NODE_ENV === 'development' && (data.platforms[0].name = 'navershopping')
      process.env.NODE_ENV === 'development' && (data.platforms[0].host = 'https://dev.apis.naver.com')
      let params = data.platforms.find(el => el.name && (el.name.toLowerCase() === this.channel))
      if (!params) return printLog({message: 'SHOPPINGLIVEChat: init chat page but no params'}, 'error')
      params.videoSeq = data.videoSeq
      if (this.isLiving && this.shoppingApi) {
        delete params.broadcastId
        printLog({message: 'SHOPPINGLIVEChat: Websocket chat connection updated configuration'})
        return this.shoppingApi.setOptions(params)
      }
      if (!params.broadcastId) return printLog({message: 'SHOPPINGLIVEChat: init chat page but no broadcastId'}, 'error')
      // start
      let storageChats = data.videoSeq ? this.readStorage(data.videoSeq) : false
      this.clearPage()
      this.prismParams = params
      this.showChats = true
      this.isLiving = true
      this.forceDisableInput = false
      this.shoppingApi = new ShoppingLiveChat({
        ...this.prismParams,
        handler: (item) => {
          switch (item.type) {
            case 'broadcast_start':
              if (!storageChats) {
                !params.isRehearsal && this.chatData.length === 0 && this.pushComment({
                  commentNo: 0,
                  id: 0,
                  message: this.$i18n.shoppingLive.liveStarted
                })
              } else {
                this.chatData = storageChats.list
                this.fixedMessage = storageChats.fixedMessage
              }
              break
            case 'broadcast_chat':
              this.handleCommentList(item.data.list)
              break
            case 'broadcast_fixed_notice':
              item.data.noticeType = 'FIXED_CHAT'
              this.handleNoticeItem(item.data)
              break
            case 'broadcast_notice':
              this.handleNoticeItem(item.data)
              break
          }
        }
      })
    },
    handlePlatformClose (data) {
      printLog({message: 'SHOPPINGLIVEChat: This chat platform is disabled'})
      if (data.platform === this.channel) {
        this.isPlatformClose = true
      }
    },
    chatReceive (data) {
      let list = data.filter(item => item.livePlatform === 'SHOPPINGLIVE')
      list.forEach(item => {
        let message = handleJson(item.rawMessage)
        if (item.shoppingLiveOption) {
          switch (item.shoppingLiveOption.type) {
            case 'NoticeFixedChat':
              message.noticeType = 'FIXED_CHAT'
              this.handleNoticeItem(message)
              break
            case 'NoticeChat':
              this.handleNoticeItem(message)
              break
            default:
              this.handleCommentList(message)
              break
          }
        } else {
          this.handleCommentList(message)
        }
      })
    },
    handleLiveEnd () {
      printLog({message: 'SHOPPINGLIVEChat: The live broadcast is over'})
      this.isLiving = false
      this.prismParams.isRehearsal && this.clearPage()
    },
    handleHourLimit () {
      this.shoppingApi && this.shoppingApi.close()
      this.shoppingApi = null
      printLog({message: 'SHOPPINGLIVEChat: Notification sending function is disabled'})
    },
    clearPage () {
      this.actionLimit = false
      this.platformClose = false
      this.showChats = false
      this.isLiving = false
      this.chatData = []
      this.prismParams = {}
      this.sendErrorList = []
      this.fixedMessage = null
      this.shoppingApi && this.shoppingApi.close()
      this.shoppingApi = null
    },
    handleDeleteFixed (item) {
      if (item._sendError) {
        this.fixedMessage = null
      } else {
        let temp = this.fixedMessage
        this.fixedMessage = null
        printLog({message: 'SHOPPINGLIVEChat: delete fixed notice'})
        this.shoppingApi.deleteNotice(item).then(() => {
          printLog({message: 'SHOPPINGLIVEChat: delete fixed notice success'})
        }).catch((err) => {
          this.fixedMessage = temp
          const {notice, alert: {confirmButton, defaultText}} = this.$i18n
          printLog({message: 'SHOPPINGLIVEChat: delete fixed notice error', data: err}, 'error')
          this.$alert({
            title: notice,
            content: defaultText,
            confirmText: confirmButton
          })
        })
      }
    },
    handleDelete (item, isSendingError) {
      if (this.$refs.chatsWrapper.isOneHourLimit) {
        return
      }
      const shoppingLiveLang = this.$i18n
      return this.$confirm({
        title: this.$i18n.notice,
        content: this.$i18n.shoppingLive.deleteText,
        confirmText: this.$i18n.confirm.deleteConfirmButton,
        cancelText: this.$i18n.confirm.cancelButton
      }).then(() => {
        if (isSendingError) {
          printLog({message: 'SHOPPINGLIVEChat: delete error send notice'}, 'error')
          this.handleDeleteSendErrorItem(item)
        } else {
          printLog({message: 'SHOPPINGLIVEChat: delete notice'})
          this.shoppingApi.deleteNotice(item).then(() => {
            let index = this.chatData.findIndex(r => r.commentNo === item.commentNo)
            if (index !== -1) {
              this.chatData.splice(index, 1)
            }
            printLog({message: 'SHOPPINGLIVEChat: delete notice success'})
          }).catch((err) => {
            const {notice, alert: {confirmButton, defaultText}} = this.$i18n
            printLog({message: 'SHOPPINGLIVEChat: delete notice error', data: err}, 'error')
            this.$alert({
              title: notice,
              content: defaultText,
              confirmText: confirmButton
            })
          })
        }
      })
    },
    handleDeleteSendErrorItem (item) {
      const index = this.sendErrorList.findIndex(el => el.commentNo === item.commentNo)
      if (index > -1) {
        this.sendErrorList.splice(index, 1)
      }
    },
    handleRetry (item) {
      if (this.$refs.chatsWrapper.isOneHourLimit) {
        return
      }
      this.handleDeleteSendErrorItem(item)
      this.handleSend(item, true)
      printLog({message: 'SHOPPINGLIVEChat: resend notice'})
    },
    resendFixedNotice (item) {
      if (item.noticeType === 'FIXED_CHAT') {
        this.fixedMessage = null
        this.handleSend(item, true)
        printLog({message: 'SHOPPINGLIVEChat: resend fixed notice'})
      }
    },
    handleSend (data, isResend) {
      if (!this.shoppingApi) return
      printLog({message: 'SHOPPINGLIVEChat: send notice'})
      this.shoppingApi.createNotice(data, isResend).then(data => {
        printLog({message: 'SHOPPINGLIVEChat: send notice success'})
      }).catch((err) => {
        const {notice, alert: {confirmButton, defaultText}} = this.$i18n
        printLog({message: 'SHOPPINGLIVEChat: send notice error', data: err}, 'error')
        this.$alert({
          title: notice,
          content: defaultText,
          confirmText: confirmButton
        }).then(() => {
          if (data.noticeType !== 'FIXED_CHAT') {
            this.sendErrorList.push({
              ...data,
              commentNo: Symbol(),
              _sendError: true,
              _isShowErrorHandleBtns: false,
              _isNotice: true
            })
          } else {
            this.fixedMessage = {
              ...data,
              commentNo: Symbol(),
              _sendError: true,
              _isShowErrorHandleBtns: false
            }
          }
        })
      })
    },
    handleCommentList (list) {
      this.pushComment(list)
      printLog({message: 'SHOPPINGLIVEChat: Receive new chat messages'})
    },
    handleNoticeItem (item) {
      item.commentNo = item.id
      if (item.noticeType === 'FIXED_CHAT') {
        if (item.deleted) {
          this.fixedMessage && this.fixedMessage.commentNo === item.commentNo && (this.fixedMessage = null)
        } else {
          this.fixedMessage = item
          printLog({message: 'SHOPPINGLIVEChat: Receive new fixed notification'})
        }
      } else {
        item._isNotice = true
        item._styleClass = ['notice-green-color', 'notice-green-yellow', 'notice-green-blue'][Math.floor(Math.random() * 3)]
        this.pushComment(item)
        printLog({message: 'SHOPPINGLIVEChat: Receive a new general notification'})
      }
    },
    pushComment (data) {
      if (!data) return
      let len = Array.isArray(data) ? data.length : 1
      if (this.chatData.length >= MAX_CACHED_CHAT_COUNT) {
        this.chatData.splice(0, len)
      }
      Array.isArray(data) ? this.chatData.push(...data) : this.chatData.push(data)
    }
  },
  mounted () {
    printLog({message: 'SHOPPINGLIVEChat: The chat page loads successfully'})
  },
  beforeDestroy () {
    printLog({message: 'SHOPPINGLIVEChat: The chat page is closed'})
  }
}
</script>
