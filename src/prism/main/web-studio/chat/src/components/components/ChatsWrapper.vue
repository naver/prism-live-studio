<template>
  <!-- chat list: use slot to insert various chatting template -->
  <div class="prism-chat-container">
    <!--    <div id="logArea"></div>-->
    <div class="chart-info-container">
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
        <div v-if="isAllChannel" @click="addChannel" style="display: inline-block;">
          <div class="icon-star-btn">
            <div class="icon-star"></div>
            <span class="text">{{$i18n.addChatSource}}</span>
          </div>
        </div>
      </div>
    </div>
    <!-- send message area -->
    <form v-if="showInput" class="chart-send-container" @submit.prevent>
      <input
        v-model="message"
        class="input"
        type="text"
        :placeholder="$i18n.placeholder"
        :disabled="disableInput"
        @input="handleInput">
      <button class="icon-send" :disabled="disableSend" @click="handleSend"></button>
    </form>
  </div>
</template>

<script>
import { MAX_CACHED_CHAT_COUNT, MAX_INPUT_CHARACTER } from '@/assets/js/constants'
import { handleJson } from '@/assets/js/utils'
import timeago from '@/assets/js/timeago'

export default {
  name: 'prismChatsWrapper',
  props: {
    chat: {
      type: Array,
      default () {
        return []
      }
    },
    isAllChannel: {
      type: Boolean,
      default: false
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
    }
  },
  data () {
    return {
      showScrollToButton: false,
      needScrolllToButton: false,
      inScrolling: false,
      chatData: [],
      timer: null,
      message: '',
      isAlerting: false,
      // min-height of chatting message item, need to modify this synchronously when modifing the styles
      itemHeight: 89,
      // chrome on laptop: 'scrollTop' and 'scrollHeight - clientHeight' has number deviation with decimal point level
      deviation: 2,
      resizeTimer: 0
    }
  },
  computed: {
    disableSend () {
      return this.disableInput || this.message.length > this.maxInputCharacter
    }
  },
  watch: {
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
          if (!this.timer) {
            this.startTimer()
          }
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
    },
    showChats (val) {
      if (!val) {
        this.clearTimer()
      }
    }
  },
  methods: {
    handlePrismEvents (event) {
      let detail = event.detail
      // console.log('events',detail)
      detail = handleJson(detail)
      let {type, data} = detail
      data = handleJson(data)
      switch (type) {
        case 'init':
          this.showScrollToButton = false
          this.$emit('live-init', data)
          break
        case 'end':
          this.message = ''
          this.$emit('live-end')
          break
        case 'token':
          this.$emit('token-refresh', data)
          break
        case 'chat':
          this.$emit('chat-receive', handleJson(data.message))
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
        case 'showtab':
        case 'showTab':
          this.$emit('handle-show-tab', data)
          break
      }
    },
    handleSend () {
      if (this.disableSend) return
      const message = this.message
      if (!message.length) return
      this.message = ''
      if (!message.trim().length) return

      this.$emit('handle-send', message)
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
      }, 100)
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
    handleTimeagoRender () {
      this.chatData.forEach(el => {
        el.timeago = timeago(el[this.timeField], this.$lang)
      })
    },
    startTimer () {
      this.clearTimer()
      this.timer = setInterval(() => {
        this.handleTimeagoRender()
      }, 10000)
    },
    clearTimer () {
      if (this.timer) {
        clearInterval(this.timer)
        this.timer = null
      }
    },
    addChannel () {
      // console.log('addChannel')
      window.sendToPrism(JSON.stringify({
        type: 'addChannel',
        data: {}
      }))
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
    this.clearTimer()
  }
}
</script>

<style lang="scss">
  @import '~@/assets/css/chatsWrapper.scss';

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
</style>
