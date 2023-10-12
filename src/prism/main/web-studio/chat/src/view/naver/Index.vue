<template>
  <chats-wrapper
      ref="chatsWrapper"
      key-field="commentNo"
      time-field="modTimeGmt"
      message-field="contents"
      :chat="renderData"
      :no-live-tips="$i18n.noLiveNaver.replace('${platform}', channelName)"
      :show-chats="showChats"
      :disable-input="!showChats || isPlatformClose"
      :max-input-character="maxInputCharactor"
      @live-init="handleLiveInit"
      @live-end="handleLiveEnd"
      @live-platform-close="handlePlatformClose"
      @handle-pull-up="handlePullUp"
      @handle-cached-exceed="handleCachedExceed"
      @re-broadcast="handleReBroadcast"
      @handle-send="handleSend">
    <template v-slot:chat="{ item }">
      <template-naver
          :isVlive="isVlive"
          :isDev="isDev"
          :data="item"
          @handle-delete="handleDelete"
          @handle-report="handleReport"
          @handle-retry="handleRetry"
          @show-operation-buttons="item._isShowErrorHandleBtns = true">
      </template-naver>
    </template>
  </chats-wrapper>
</template>

<script>
import {
  CHANNEL,
  CHANNEL_NAME,
  CHANNEL_ON_ALL_PAGE,
  MAX_CACHED_CHAT_COUNT,
  MAX_INPUT_CHARACTER_VLIVE,
  MAX_INPUT_CHARACTER_NAVERTV
} from '@/assets/js/constants'
import timeago from '@/assets/js/timeago'
import BufferPool from '@/assets/js/buffer'
import { handleJson } from '@/assets/js/utils'
import { CommentManage } from '@/fetch/comment'
import ChatsWrapper from '@/components/components/ChatsWrapper.vue'
import TemplateNaver from '@/components/template/Naver.vue'
import printLog from '@/assets/js/printLog'

export default {
  name: 'prismNaver',
  components: {
    ChatsWrapper, TemplateNaver
  },
  data () {
    return {
      channel: '',
      chatData: [],
      isLiving: false,
      showChats: false,
      prismParams: {},
      commentManager: null,
      reportedIDs: [],
      sendErrorList: [],
      noMorePreviousComment: false,
      pageSize: 50,
      pollTime: 5000,
      showLoading: false,
      bufferManager: null,
      isPlatformClose: false,
      isDev: false
    }
  },
  computed: {
    isVlive () {
      return this.channel === CHANNEL.VLIVE
    },
    renderData () {
      return this.chatData.concat(this.sendErrorList)
    },
    maxInputCharactor () {
      return this.isVlive ? MAX_INPUT_CHARACTER_VLIVE : MAX_INPUT_CHARACTER_NAVERTV
    },
    channelName () {
      return this.channel && CHANNEL_NAME[this.channel]
    }
  },
  methods: {
    handleLiveInit (data) {
      if (this.isLiving) return
      this.clearPage()
      if (!data) return
      if (data.platforms) {
        this.prismParams = data.platforms.find(el => el.name === this.channel)
        this.isDev = this.prismParams.GLOBAL_URI.indexOf('dev-') !== -1
        if (this.prismParams) {
          printLog({message: this.channel + 'Chat: Get the initialization data'})
          if (this.prismParams.name === CHANNEL.VLIVE && !this.prismParams.objectId) {
            printLog({message: this.channel + 'Chat: vlive need objectId but no objectId'}, 'error')
            return
          }
          if (this.prismParams.isRehearsal) return
          this.isPlatformClose = this.prismParams.disable ? true : false
          this.showChats = true
          this.isLiving = true
          this.handleCommentManagerInit()
          this.bufferManager = new BufferPool(this.chatData, {
            pollTime: this.pollTime
          })
        }
      }
    },
    handlePlatformClose (data) {
      if (data.platform === this.channel) {
        printLog({message: this.channel + 'Chat: This chat platform is disabled'})
        this.isPlatformClose = true
      }
    },
    handleLiveEnd () {
      printLog({message: this.channel + 'Chat: The live broadcast is over'})
      this.isLiving = false
    },
    handleCommentManagerInit () {
      if (!this.commentManager) {
        this.commentManager = new CommentManage({
          type: this.channel,
          pollTime: this.pollTime,
          pageSize: this.pageSize,
          ...this.prismParams
        })
        this.commentManager.on('NEW_COMMENTS', data => {
          switch (data.type) {
            case 'last':
              this.handlePreviousComment(data.list)
              break
            case 'next':
              this.handleImmediateComment(data.list)
              break
          }
        })
        this.commentManager.start()
      }
    },
    handlePreviousComment (data) {
      if (data.length) {
        let res = this.filterCommentData(data)
        if (res.length) {
          this.chatData.push(...res)
        }
      }
    },
    handleImmediateComment (data) {
      if (data.length) {
        let res = this.filterCommentData(data, true)
        if (res.length) {
          this.bufferManager.push(res)
          res.length && printLog({message: this.channel + 'Chat: Receive new chat'})
        }
      }
    },
    filterCommentData (data, immediate) {
      let res = []
      let list = data.reverse()
      const now = new Date()
      list.forEach(el => {
        if (!this.reportedIDs.includes(el.userIdNo)) {
          if (el.extension) {
            el.extension = handleJson(decodeURIComponent(el.extension))
          }
          const {isSelf, isManager} = this.getUserPermission(el)
          el._isSelf = isSelf
          el._isManager = isManager
          if (immediate) {
            el.timeago = timeago(now, this.$lang)
          } else {
            el.timeago = timeago(el.modTimeGmt, this.$lang)
          }
          res.push(el)
        }
      })
      return res
    },
    clearPage () {
      this.platformClose = false
      this.showChats = false
      this.chatData = []
      this.prismParams = {}
      this.reportedIDs = []
      this.sendErrorList = []
      this.noMorePreviousComment = false
      this.clearCommentManager()
      this.clearBufferManager()
    },
    clearCommentManager () {
      if (this.commentManager) {
        this.commentManager.stop()
        this.commentManager = null
      }
    },
    clearBufferManager () {
      if (this.bufferManager) {
        this.bufferManager.clearTimer()
        this.bufferManager = null
      }
    },
    handleReport (item) {
      const {notice, confirm: {reportText, cancelButton}, alert: {confirmButton, defaultText}} = this.$i18n
      this.$confirm({
        title: notice,
        content: reportText,
        confirmText: confirmButton,
        cancelText: cancelButton
      }).then(() => {
        printLog({message: this.channel + 'Chat: Report request for comment start'})
        this.commentManager.request.repComment(item.commentNo).then(() => {
          this.reportedIDs.push(item.userIdNo)
          const filtered = this.chatData.filter(el => el.userIdNo !== item.userIdNo)
          this.chatData.length = 0
          this.chatData.push(...filtered)
          if (CHANNEL_ON_ALL_PAGE.includes(this.channel)) {
            window.sendToPrism(JSON.stringify({
              type: 'broadcast',
              data: {
                receive: 'ALL',
                type: 'report',
                data: item.userIdNo
              }
            }))
          }
          printLog({message: this.channel + 'Chat: Report request for comment success'})
        }).catch(e => {
          this.$alert({
            title: notice,
            content: defaultText,
            confirmText: confirmButton
          }).then(() => {})
          printLog({message: this.channel + 'Chat: Report request for comment error'}, 'error', e)
        })
      }).catch(() => {})
    },
    handleDelete (item, isSendingError) {
      const {notice, confirm: {deleteText, deleteConfirmButton, cancelButton}, alert: {defaultText, confirmButton}} = this.$i18n
      this.$confirm({
        title: notice,
        content: deleteText,
        confirmText: deleteConfirmButton,
        cancelText: cancelButton
      }).then(() => {
        if (isSendingError) {
          this.handleDeleteSendErrorItem(item)
        } else {
          printLog({message: this.channel + 'Chat: delete chat request start'})
          this.commentManager.request.delComment(item.commentNo).then(() => {
            const index = this.chatData.indexOf(item)
            if (index > -1) {
              this.chatData.splice(index, 1)
            }
            if (CHANNEL_ON_ALL_PAGE.includes(this.channel)) {
              window.sendToPrism(JSON.stringify({
                type: 'broadcast',
                data: {
                  receive: 'ALL',
                  type: 'delete',
                  data: item.commentNo
                }
              }))
            }
            printLog({message: this.channel + 'Chat: Request to delete comment success'})
          }).catch(e => {
            this.$alert({
              title: notice,
              content: defaultText,
              confirmText: confirmButton
            }).then(() => {})
            printLog({message: this.channel + 'Chat: Request to delete comment error', data: e}, 'error')
          })
        }
      }).catch(() => {})
    },
    handleDeleteSendErrorItem (item) {
      const index = this.sendErrorList.findIndex(el => el.commentNo === item.commentNo)
      if (index > -1) {
        this.sendErrorList.splice(index, 1)
      }
    },
    handleSend (data) {
      printLog({message: this.channel + 'Chat: start send chat'})
      this.commentManager.request.sendComment({
        commentType: 'txt',
        contents: data
      }).then(res => {
        printLog({message: this.channel + 'Chat: send chat success'})
        this.chatData.push(...this.filterCommentData(res))
      }).catch((err) => {
        printLog({message: this.channel + 'Chat: send chat error', data: err}, 'error')
        const {notice, alert: {confirmButton, defaultText, sensitiveText}} = this.$i18n
        if (err.code === '5020') {
          this.$alert({
            title: notice,
            content: sensitiveText,
            confirmText: confirmButton
          })
        } else {
          this.$alert({
            title: notice,
            content: defaultText,
            confirmText: confirmButton
          }).then(() => {
            const prismParams = this.prismParams
            const now = new Date()
            this.sendErrorList.push({
              commentNo: Symbol(),
              modTimeGmt: now,
              _isSelf: true,
              _isManager: false,
              timeago: timeago(now, this.$lang),
              extension: '',// prismParams.extension,
              userName: prismParams.username,
              userProfileImage: prismParams.userProfileImage,
              contents: data,
              _sendError: true,
              _isShowErrorHandleBtns: false,
              _isSending: false
            })
          })
        }
      })
    },
    handleRetry (item) {
      if (item._isSending) return
      item._isSending = true
      printLog({message: this.channel + 'Chat: start resend chat'})
      this.commentManager.request.sendComment({
        commentType: 'txt',
        contents: item.contents
      }).then(res => {
        printLog({message: this.channel + 'Chat: resend chat success'})
        this.handleDeleteSendErrorItem(item)
        this.chatData.push(...this.filterCommentData(res))
      }).catch(err => {
        printLog({message: this.channel + 'Chat: resend chat error', data: err}, 'error')
        const {notice, alert: {confirmButton, defaultText}} = this.$i18n
        this.$alert({
          title: notice,
          content: defaultText,
          confirmText: confirmButton
        }).then(() => {})
        item._isShowErrorHandleBtns = false
      }).finally(() => {
        item._isSending = false
      })
    },
    getUserPermission (item) {
      let isSelf = false
      let isManager = false
      switch (this.channel) {
        case CHANNEL.VLIVE:
          isSelf = item.mine
          isManager = !isSelf && item.manager
          break
        case CHANNEL.NAVERTV:
          isSelf = this.prismParams.userId === item.userIdNo
          isManager = !isSelf && item.manager
          break
      }
      return {isSelf, isManager}
    },
    handlePullUp () {
      if (this.noMorePreviousComment || this.showLoading) {
        return
      }
      this.showLoading = true
      const oldestComment = this.chatData[0]
      printLog({message: this.channel + 'Chat: getOlderComment chat start'})
      this.commentManager.request.getOlderComment(oldestComment.commentNo, this.pageSize).then(data => {
        this.showLoading = false
        if (!data.length) {
          this.noMorePreviousComment = true
          return
        }
        let res = this.filterCommentData(data)
        if (res.length) {
          this.chatData.unshift(...res)
          this.$nextTick(() => {
            this.$refs.chatsWrapper && this.$refs.chatsWrapper.$refs.scroller.scrollToItem(res.length - 1)
          })
        }
        printLog({message: this.channel + 'Chat: getOlderComment chat success'})
      }).catch((e) => {
        printLog({message: this.channel + 'Chat: getOlderComment chat error', data: e}, 'error')
        this.showLoading = false
      })
    },
    handleCachedExceed () {
      const length = this.chatData.length
      if (length <= MAX_CACHED_CHAT_COUNT) return
      const count = length - MAX_CACHED_CHAT_COUNT + MAX_CACHED_CHAT_COUNT * 0.1
      this.chatData.splice(0, count)
      this.noMorePreviousComment = false
    },
    handleReBroadcast (data) {
      data = handleJson(data)
      if (data.receive === this.channel) {
        switch (data.type) {
          case 'delete':
            const index = this.chatData.findIndex(el => {
              return el.commentNo === data.data
            })
            index !== -1 && this.chatData.splice(index, 1)
            break
        }
      }
    }
  },
  created () {
    let platform = new URLSearchParams(document.location.search).get('platform') || ''
    this.channel = platform.toLocaleUpperCase()
    if (![CHANNEL.NAVERTV, CHANNEL.VLIVE].includes(this.channel)) {
      throw new Error('Please take the correct platform name on url')
    }
  },
  mounted () {
    printLog({message: this.channel + 'Chat: The chat page loads successfully'})
  },
  beforeDestroy () {
    printLog({message: this.channel + 'Chat: The chat page is closed'})
    this.clearCommentManager()
    this.clearBufferManager()
  }
}
</script>
