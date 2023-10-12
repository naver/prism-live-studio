<template>
  <!-- chat list: use slot to insert various chatting template -->
  <div class="prism-chat-container" style="padding-top: 0" :class="{'has-fixed-chat-padding-top':fixedMessage}">
    <div ref="$fixed" @mouseenter="mouseEnterFixedArea" @mouseleave="mouseLeaveFixedArea" v-if="fixedMessage" class="fixed-notice-area">
      <button v-if="!fixedMessage._sendError&&!isOneHourLimit"
              :data-title="$i18n.tooltip.deleteNotice"
              class="fixed-del-btn"
              @click="handleDeleteFixed"></button>
      <shopping
          @handle-delete="handleDeleteFixed"
          @handle-retry="$emit('retry-fixed-notice',fixedMessage)"
          @show-operation-buttons="fixedMessage._isShowErrorHandleBtns = true"
          ref="$fixedItem"
          style="padding: 10px 28px 10px 20px;"
          :isMouseOverFixedArea="isMouseOverFixedArea"
          is-fixed
          :disable-btn="isOneHourLimit"
          :data="fixedMessage"></shopping>
    </div>
    <!--    <div id="logArea"></div>-->
    <div class="chart-info-container" :style="{'--container-h':containerHeight}" :class="{'has-fixed-chat-height':fixedMessage}">
      <template v-if="showChats">
        <template v-if="chatData.length">
          <dynamic-scroller
              ref="scroller"
              class="chat-info-scrollbar"
              :items="chatData"
              :min-item-size="itemHeight"
              :key-field="keyField"
              :buffer="1000"
              @scroll.native.passive="handleScroll">
            <template v-slot="{ item, index, active }">
              <dynamic-scroller-item
                  :item="item"
                  :active="active"
                  :size-dependencies="[
                  item[messageField]
                ]">
                <slot name="chat" :item="item"></slot>
              </dynamic-scroller-item>
            </template>
          </dynamic-scroller>
          <button class="scroll-to-bottom"
                  v-if="showScrollToButton"
                  @click="scrollToBottom"></button>
        </template>
      </template>
      <!-- tip: live hasn't been starting -->
      <div v-else-if="noLiveTips" class="tips-no-video">
        <i class="icon-chat"></i><br>
        <span v-html="noLiveTips"></span><br>
      </div>
    </div>
    <!-- send message area -->
    <form v-if="showInput" class="chart-send-container" @submit.prevent>
      <input
          ref="$messageInput"
          v-model="message"
          class="input"
          type="text"
          @focus="handleInputFocus"
          @focusout="inputFocus = false"
          :placeholder="$i18n.shoppingLive.placeholder"
          :disabled="disableInput"
          @input="handleInput">
      <div class="is-fixed" :class="{'is-check':isFixed,'disabled':disableInput}" @click="!disableInput&&(isFixed = !isFixed)">
        <div class="is-fixed-box"></div>
        <div class="is-fixed-text">{{ $i18n.shoppingLive.pin }}</div>
      </div>
      <button class="icon-send" :class="{'send-btn-focus':(inputFocus||message.length>0)&&!disableSend}" :disabled="disableSend" @click="handleSend"></button>
    </form>
  </div>
</template>

<script>
import { MAX_CACHED_CHAT_COUNT, MAX_INPUT_CHARACTER } from '@/assets/js/constants'
import { handleJson, testCommentItem } from '@/assets/js/utils'
import timeago from '@/assets/js/timeago'
import Shopping from '@/components/template/Shopping.vue'

export default {
  name: 'prismChatsWrapper',
  components: {Shopping},
  props: {
    chat: {
      type: Array,
      default () {
        return []
      }
    },
    showChats: Boolean,
    noLiveTips: String,
    keyField: {
      type: String,
      default: 'id'
    },
    timeField: {
      type: String,
      default: 'publishedAt'
    },
    messageField: {
      type: String,
      default: 'message'
    },
    showInput: {
      type: Boolean,
      default: true
    },
    disableInput: {
      type: Boolean
    },
    maxInputCharacter: {
      type: Number,
      default: MAX_INPUT_CHARACTER
    },
    fixedMessage: {
      type: Object,
      default: null
    }
  },
  data () {
    return {
      showScrollToButton: false,
      needScrolllToButton: false,
      inScrolling: false,
      chatData: [],
      message: '',
      isAlerting: false,
// min-height of chatting message item, need to modify this synchronously when modifing the styles
      itemHeight: 89,
// chrome on laptop: 'scrollTop' and 'scrollHeight - clientHeight' has number deviation with decimal point level
      deviation: 2,
      resizeTimer: 0,
      isFixed: true,
      inputFocus: false,
      isMouseOverFixedArea: false,
      isOneHourLimit: false,
      hourLimitTimer: 0,
      prismInitData: {},
      containerHeight: 'calc(100% - 123px)'
    }
  },
  computed: {
    disableSend () {
      return this.disableInput || this.message.length > this.maxInputCharacter
    }
  },
  watch: {
    isOneHourLimit (now) {
      this.$emit('change-limit', now)
    },
    fixedMessage () {
      this.isMouseOverFixedArea = false
      this.$nextTick(() => {
        let fixedH = (this.$refs.$fixed && this.$refs.$fixed.offsetHeight) || 0
        this.containerHeight = fixedH > 40 ? 'calc(100% - 123px)' : 'calc(100% - 102px)'
        this.$nextTick(() => {
          this.handleResize()
        })
      })
    },
    disableInput (now, old) {
      now && (this.message = '')
    },
    chat (val) {
      if (Array.isArray(val)) {
        this.chatData = val
        if (!this.showScrollToButton && val.length > MAX_CACHED_CHAT_COUNT) {
          if (this.$listeners['handle-cached-exceed']) {
            this.$emit('handle-cached-exceed')
          }
        }
        if (val.length) {
          this.needScrolllToButton = !this.showScrollToButton
          if (this.needScrolllToButton) {
            setTimeout(() => {
              this.scrollToBottom()
            })
          }
        }
      }
      this.$nextTick(() => {
        this.handleResize()
      })
    }
  },
  methods: {
    mouseEnterFixedArea () {
      this.isMouseOverFixedArea = true
    },
    mouseLeaveFixedArea () {
      this.isMouseOverFixedArea = false
    },
    handlePrismEvents (event) {
      let detail = event.detail
      detail = handleJson(detail)
      let {type, data} = detail
      data = handleJson(data)
      switch (type) {
        case 'init':
          clearTimeout(this.hourLimitTimer)
          this.isOneHourLimit = false
          this.showScrollToButton = false
          this.prismInitData = data.platforms[0]
          this.$emit('live-init', data)
          break
        case 'end':
          this.message = ''
          this.isOneHourLimit = false
          let isDev = new URLSearchParams(document.location.search).get('dev') === '1'
          let delayCloseTime = this.prismInitData.isIIMS ? 50 * 1000 * 60 : 0
          delayCloseTime = isDev ? 3 * 1000 * 60 : delayCloseTime
          this.hourLimitTimer = setTimeout(() => {
            this.isOneHourLimit = true
            this.$refs.$messageInput && this.$refs.$messageInput.blur()
            this.$emit('one-hour-limit')
          }, delayCloseTime)
          this.$emit('live-end')
          this.prismInitData.isRehearsal && (this.isFixed = true)
          break
        case 'token':
          this.$emit('token-refresh', data)
          break
        case 'chat':
          // this.$emit('chat-receive', handleJson(data.message))
          break
        case 'platform_close':
          this.$emit('live-platform-close', data)
          break
        case 'broadcast':
          this.$emit('re-broadcast', data)
          break
        case 'permission':
          this.$emit('handle-permission', data)
          break
      }
    },
    handleSend () {
      if (this.disableSend) return
      const message = this.message
      if (!message.length) return
      this.message = ''
      if (!message.trim().length) return
      this.$emit('handle-send', {
        message,
        noticeType: this.isFixed ? 'FIXED_CHAT' : 'CHAT'
      })
      this.scrollToBottom()
    },
    handleInput (e) {
      if (this.isAlerting) return
      const value = e.target.value
      const maxInputCharacter = this.maxInputCharacter
      if (value.length > maxInputCharacter) {
        const {notice, alert: {text, confirmButton}} = this.$i18n
        this.isAlerting = true
        this.$alert({
          title: notice,
          content: text.replace('${length}', maxInputCharacter),
          confirmText: confirmButton
        }).then(() => {
          this.isAlerting = false
          this.message = value.slice(0, maxInputCharacter)
        })
      }
    },
    handleResize () {
      clearTimeout(this.resizeTimer)
      this.resizeTimer = setTimeout(() => {
        const scroller = this.$refs.scroller && this.$refs.scroller.$el
        if (scroller) {
          this.showScrollToButton = !this.needScrolllToButton && (scroller.scrollHeight - scroller.clientHeight - this.deviation > scroller.scrollTop)
        }
        this.$refs.$fixedItem && this.$refs.$fixedItem.computedFxiedArea()
      }, 100)
    },
    handleDeleteFixed () {
      if (this.isOneHourLimit) {
        return
      }
      this.$emit('handle-delete-fixed', this.fixedMessage)
    },
    handleScroll (event) {
      const scroller = event.target
      if (!scroller) return
      this.showScrollToButton = !this.needScrolllToButton && (scroller.scrollHeight - scroller.clientHeight - this.deviation > scroller.scrollTop)

      if (this.$listeners['handle-pull-up']) {
        if (scroller.scrollTop <= 0) {
          this.$emit('handle-pull-up')
        }
      }
    },
    scrollToBottom () {
      if (this.inScrolling) return
      const scroller = this.$refs.scroller && this.$refs.scroller.$el
      if (!scroller) {
        this.needScrolllToButton = false
        return
      }
      let scrolltarget = scroller.scrollHeight - scroller.clientHeight
      if (scrolltarget - this.deviation <= scroller.scrollTop) {
        this.needScrolllToButton = false
        return
      }
      const MAXSTEP = 10000
      const offset = scrolltarget - scroller.scrollTop
      this.inScrolling = true
      let step = Math.max(offset > 1000 ? offset / 1000 : offset / 100, 10)
      const doSmoothScroll = () => {
// scrollHeight and clientHeight will be changed at any time in virtual list
        scrolltarget = scroller.scrollHeight - scroller.clientHeight
        if (scroller.scrollTop < scrolltarget - this.deviation) {
          scroller.scrollTop = Math.ceil(scroller.scrollTop + step)
          step = Math.min(MAXSTEP, step * 1.1)
          requestAnimationFrame(doSmoothScroll)
        } else {
          scroller.scrollTop = scrolltarget
          this.inScrolling = false
          this.needScrolllToButton = false
        }
      }
      doSmoothScroll()
    },
    handleInputFocus (event) {
      if (this.isOneHourLimit) {
        event.target.blur()
        const {notice, alert: {text, confirmButton}} = this.$i18n
        this.isAlerting = true
        this.$alert({
          title: notice,
          content: this.$i18n.shoppingLive.noticeCantSend,
          confirmText: confirmButton
        }).then(() => {
          this.isAlerting = false
          this.inputFocus = false
          this.message = ''
        })
      } else {
        this.inputFocus = true
      }
    }
  },
  mounted () {
    window.addEventListener('resize', this.handleResize)
  },
  created () {
    window.addEventListener('prism_events', this.handlePrismEvents)
  },
  beforeDestroy () {
    window.removeEventListener('prism_events', this.handlePrismEvents)
    window.removeEventListener('resize', this.handleResize)
  }
}
</script>

<style lang="scss" scoped>
@import '~@/assets/css/chatsWrapper.scss';

.prism-chat-container {
  overflow: hidden;
}

button[data-title]:hover:after {
  content: attr(data-title);
  position: absolute;
  width: max-content;
  top: 9px;
  right: 18px;
  padding: 2px 4px;
  border: 1px solid #000000;
  border-radius: 1px;
  background-color: #111111;
  color: #ffffff;
}

#logArea {
  z-index: 100;
  position: absolute;
  top: 0;
  left: 0;
  height: 100%;
  width: 50%;
  background: rgba(0, 0, 0, 0.3);
  color: rgba(255, 255, 255, .5)
}

.icon-star-btn {
  font-size: 12px;
  display: flex;
  align-items: center;
  justify-content: center;
  margin-top: 12px;
  height: 27px;
  line-height: 27px;
  border-radius: 3px;
  border: solid 1px #3d3d3d;
  padding: 0 12px;
  user-select: none;
  color: #bababa;

  &:hover {
    border: solid 1px #A2A2A2;
    color: #A2A2A2;

    .icon-star {
      background: url(~@/assets/img/chat-star-hover.svg) no-repeat;
    }
  }

  &:active {
    border: solid 1px #333333;
    color: #333333;

    .icon-star {
      background: url(~@/assets/img/chat-star-active.svg) no-repeat;
    }
  }

  .icon-star {
    display: inline-block;
    width: 15px;
    line-height: 15px;
    height: 15px;
    background: url(~@/assets/img/chat-star.svg) no-repeat;
    background-size: 100%;
    margin-right: 4px;
  }

  .text {
    display: inline-block;
    font-weight: bold;
  }
}

.is-fixed {
  position: absolute;
  width: 50px;
  height: 22px;
  top: 14px;
  right: 35px;
  display: flex;
  align-items: center;
  cursor: pointer;

  &.disabled {
    cursor: not-allowed;
  }

  &.disabled {
    opacity: .6;
  }

  .is-fixed-box {
    width: 15px;
    height: 15px;
    padding: 0 0 0 0;
    object-fit: contain;
    background: url(~@/assets/img/fixed-unchekedbox.png) no-repeat;
    background-size: cover;
  }

  .is-fixed-text {
    width: 24px;
    height: 14px;
    font-size: 12px;
    line-height: 12px;
    font-weight: normal;
    font-stretch: normal;
    font-style: normal;
    letter-spacing: normal;
    color: #bababa;
    user-select: none;
    margin-left: 5px;
  }

  &.is-check .is-fixed-box {
    background: url(~@/assets/img/fixed-chekedbox.png) no-repeat;
    background-size: cover;
  }
}

.chart-send-container .input {
  padding: 12px 95px 12px 20px;
}

.has-fixed-chat-height {
  --container-h: calc(100% - 123px);
  height: var(--container-h);
  margin-top: 5px;
}

.has-fixed-chat-padding-top {
  padding-top: 0;
}

.fixed-notice-area {
  background: white;

  .fixed-del-btn {
    z-index: 10;
    cursor: pointer;
    right: 7px;
    position: absolute;
    top: 10px;
    width: 16px;
    height: 16px;
    background: url(~@/assets/img/btn-close-normal.svg) no-repeat;
    background-size: 100%;
    display: none;
  }

  &:hover {
    .fixed-del-btn {
      display: block;
    }
  }
}

.fixed-message-item {
  font-family: MalgunGothic;
  font-size: 14px;
  font-weight: bold;
  font-stretch: normal;
  font-style: normal;
  line-height: normal;
  letter-spacing: normal;
  color: #2d2d2d;
}

.send-btn-focus {
  background-image: url(~@/assets/img/chatsend-on-normal.svg) !important;
}
</style>
