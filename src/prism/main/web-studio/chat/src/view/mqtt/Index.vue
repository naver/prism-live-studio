<template>
  <chats-wrapper
      :chat="chatData"
      :show-chats="showChats"
      :no-live-tips="guideText"
      :max-input-character="maxInputCharactor"
      :disable-input="isFacebook || !showChats || !isLiving||isPlatformClose"
      @live-init="handleLiveInit"
      @live-end="handleLiveEnd"
      @live-platform-close="handlePlatformClose"
      @chat-receive="handleChat"
      @handle-send="handleSend"
      @handle-show-tab="showFacebookToast"
      @handle-permission="handlePermission">
    <template v-slot:chat="{ item }">
      <component :is="temp" :data="item" no-channel-image></component>
    </template>
  </chats-wrapper>
</template>

<script>
import {
  CHANNEL,
  CHANNEL_NAME,
  MAX_LOCAL_CACHED_CHAT_COUNT,
  MAX_INPUT_CHARACTER_AFREECATV
} from '@/assets/js/constants'
import { handleJson } from '@/assets/js/utils'
import timeago from '@/assets/js/timeago'
import BufferPool from '@/assets/js/buffer'
import AfreecaTV from '@/components/components/afreecaTV'
import ChatsWrapper from '@/components/components/ChatsWrapper.vue'
import TemplateNormal from '@/components/template/Normal.vue'
import TemplateAfreeca from '@/components/template/AfreecaTV.vue'
import Toast from '@/components/toast'
import printLog from '@/assets/js/printLog'

export default {
  name: 'prismMqtt',
  mixins: [AfreecaTV],
  components: {
    ChatsWrapper,
    TemplateNormal,
    TemplateAfreeca
  },
  data () {
    return {
      channel: '',
      isLiving: false,
      showChats: false,
      chatData: [],
      prismParams: {},
      pollTime: 5000,
      bufferManager: null,
      maxInputCharactor: MAX_INPUT_CHARACTER_AFREECATV,
      isPlatformClose: false,
      isShowFbToast: false
    }
  },
  watch: {
    chatData (now) {
      this.isLiving && localStorage.setItem(this.channel, JSON.stringify({
        videoSeq: this.prismParams.videoSeq,
        list: now.slice(0, MAX_LOCAL_CACHED_CHAT_COUNT)
      }))
    }
  },
  computed: {
    isFacebook () {
      return this.channel === CHANNEL.FACEBOOK
    },
    isAfreecaTV () {
      return this.channel === CHANNEL.AFREECATV
    },
    channelName () {
      return this.channel && CHANNEL_NAME[this.channel]
    },
    guideText () {
      if (this.isLiving) {
        return this.$i18n['forbidden' + this.channel]
      } else {
        return this.$i18n.noLiveNaver.replace('${platform}', this.channelName)
      }
    },
    temp () {
      return this.isAfreecaTV ? TemplateAfreeca : TemplateNormal
    }
  },
  methods: {
    handlePlatformClose (data) {
      if (data.platform === this.channel) {
        printLog({message: this.channel + 'Chat: This chat platform is disabled'})
        this.isPlatformClose = true
      }
    },
    readStorage (videoSeq) {
      printLog({message: this.channel + `Chat: read chat storage`})
      let storage = localStorage.getItem(this.channel) || null
      typeof storage === 'string' && (storage = JSON.parse(storage))
      localStorage.removeItem(this.channel)
      if (!storage || storage.videoSeq !== videoSeq) {
        printLog({message: this.channel + `Chat: read chat storage but no chat data`})
        return false
      }
      printLog({message: this.channel + `Chat: read chat storage success, storage chat length${storage.list.length}`})
      storage.list.forEach(i => i.id = Symbol())
      return storage.list
    },
    handleLiveInit (data) {
      if (this.isLiving) return
      this.clearPage()
      if (!data) return
      if (data.platforms) {
        this.prismParams = data.platforms.find(el => el.name === this.channel)
        this.prismParams.videoSeq = data.videoSeq
        if (this.prismParams) {
          printLog({message: this.channel + 'Chat: Get the initialization data'})
          this.isLiving = true
          this.bufferManager = new BufferPool(this.chatData, {
            pollTime: this.pollTime
          })

          if (!this.prismParams.isPrivate) {
            this.showChats = true
            this.showFacebookToast({platform: this.channel})
          }

          if (this.prismParams.videoSeq) {
            let storageList = this.readStorage(this.prismParams.videoSeq)
            storageList && (this.chatData = storageList)
          }
        }
      }
    },
    showFacebookToast (data) {
      if (this.isLiving && data.platform === this.channel && !this.isShowFbToast && this.isFacebook && !this.prismParams.isPrivate) {
        Toast(this.$i18n.facebookNoSend, {
          position: {
            bottom: '60px'
          }
        })
        this.isShowFbToast = true
      }
    },
    handleLiveEnd () {
      printLog({message: this.channel + 'Chat: The live broadcast is over'})
      this.isLiving = false
    },
    handleChat (data) {
      if (!this.isLiving) return
      const res = []
      data.forEach(el => {
        if (el.livePlatform === this.channel) {
          if (this.isFacebook && this.prismParams.isPrivate) {
            this.handlePermission({
              platform: CHANNEL.FACEBOOK,
              isPrivate: false
            })
          }

          el.id = Symbol()
          el.timeago = timeago(el.publishedAt, this.$lang)
          if (this.isAfreecaTV) {
            let handledData = this.handleAfreecatvChat(el, this.prismParams.userId)
            handledData && res.push(handledData)
          } else if (this.isFacebook) {
            let raw = el.rawMessage = handleJson(el.rawMessage)
            el._isSelf = raw.from.id && (+raw.from.id === +this.prismParams.userId)
            let attachment = el.rawMessage.attachment || {}
            if (el.message) {
              res.push(el)
            } else if (attachment.type === 'animated_image_share' && attachment.media.source) {
              res.push(el)
            } else if (attachment.type === 'sticker') {
              res.push(el)
            }
          } else {
            el._isSelf = el.author.isChatOwner
            res.push(el)
          }
        }
      })
      this.bufferManager.push(res)
      res.length && printLog({message: this.channel + 'Chat: Receive new chat'})
    },
    handleSend (data) {
      printLog({message: this.channel + 'Chat: send chat'})
      window.sendToPrism(JSON.stringify({
        type: 'send',
        data: {
          platform: this.channel,
          message: data
        }
      }))
    },
    handlePermission (data) {
      if (!this.isLiving) return
      if (this.channel === data.platform) {
        printLog({message: this.channel + 'Chat: Update chat permission data'})
        if (data.isPrivate && !this.prismParams.isPrivate && this.isFacebook) {
          Toast(this.$i18n.forbiddenFACEBOOK, {
            position: {
              top: '10px'
            }
          })
        }
        this.prismParams.isPrivate = data.isPrivate
        if (!data.isPrivate) {
          this.showChats = true
          this.showFacebookToast({platform: this.channel})
        } else if (this.isAfreecaTV) {
          this.showChats = false
        }
      }
    },
    clearPage () {
      this.isPlatformClose = false
      this.isLiving = false
      this.showChats = false
      this.chatData = []
      this.prismParams = {}
      this.isShowFbToast = false
      this.clearBufferManager()
    },
    clearBufferManager () {
      if (this.bufferManager) {
        this.bufferManager.clearTimer()
        this.bufferManager = null
      }
    }
  },
  created () {
    let platform = new URLSearchParams(document.location.search).get('platform') || ''
    this.channel = platform.toLocaleUpperCase()
    if (![CHANNEL.FACEBOOK, CHANNEL.AFREECATV].includes(this.channel)) {
      printLog({message: this.channel + 'Chat: Did not get the correct Name on the URL'}, 'error')
    }
  },
  mounted () {
    printLog({message: this.channel + 'Chat: The chat page loads successfully'})
  },
  beforeDestroy () {
    printLog({message: this.channel + 'Chat: The chat page is closed'})
    this.clearBufferManager()
  }
}
</script>

<style>
.prism-toast {
  position: absolute;
  bottom: 50px;
  left: 0;
}
</style>
