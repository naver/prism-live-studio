<template>
  <div class="chat-info-item naver-chat-item shopping-item" :class="{'is-notice-error':data._sendError,'is-start-message':data.commentNo===0,'fixed-notice-item':isFixed,'notice':!isFixed&&isNotice}">
    <div class="chat-content">
      <div class="title" v-if="!isNotice&&!data.commentNo=='0'" style="margin-bottom: 7px;">
        <div class="author-name">
          <span>{{data.nickname}}</span>
        </div>
      </div>
      <div class="message-box" :class="{'start-message':data.commentNo===0}">
        <span class="message-content" :class="{'notice-message-content':isNotice,
              'fixed-message-text':isFixed,
              [data._styleClass]:!isFixed&&isNotice}"
        >{{data.message}}</span>
        <span ref="$messageTextTemp"
              class="fixed-message-temp"
        >{{data.message}}</span>
      </div>
      <div class="chat-btns" v-if="isNotice&&!disableBtn">
        <template v-if="data._sendError">
          <div style="display: flex;align-items: center;" v-if="data._isShowErrorHandleBtns">
            <button class="btn-retry" @click="handleRetry" :data-title="$i18n.tooltip.retry"></button>
            <btn-close is-small @click="handleDelete(true)" :data-title="$i18n.tooltip.deleteNotice"></btn-close>
          </div>
          <button v-else class="btn-send-error" :data-title="$i18n.tooltip.sendError" @click="showButtons"></button>
        </template>
        <btn-close class="chat-btn-close" :data-title="$i18n.tooltip.deleteNotice" is-small @click="handleDelete"></btn-close>
      </div>
    </div>
    <div v-if="hasOpenArea" class="fixed-open-area" v-show="isMouseOverFixedArea">
      {{data.message}}
    </div>
  </div>
</template>

<script>
import BtnClose from '@/components/BtnClose.vue'
import { testCommentItem } from '@/assets/js/utils'

export default {
  name: 'templateShopping',
  components: {BtnClose},
  props: {
    data: {
      type: Object,
      default () {
        return {}
      }
    },
    isFixed: {
      type: Boolean,
      default: false
    },
    disableBtn: {
      type: Boolean,
      default: false
    },
    isMouseOverFixedArea: {
      type: Boolean,
      default: false
    },
    isDev: Boolean
  },
  data () {
    return {
      hasOpenArea: false
    }
  },
  computed: {
    isNotice () {
      return this.isFixed || this.data._isNotice
    }
  },
  watch: {
    'data.message' () {
      this.$nextTick(() => {
        this.isFixed && this.computedFxiedArea()
      })
    }
  },
  methods: {
    computedFxiedArea () {
      let textH = this.$refs.$messageTextTemp && this.$refs.$messageTextTemp.offsetHeight
      this.hasOpenArea = textH && textH > 80
    },
    handleReport () {
      this.$emit('handle-report', this.data)
    },
    handleDelete (isRetry) {
      this.$emit('handle-delete', this.data, isRetry)
    },
    handleRetry () {
      this.$emit('handle-retry', this.data)
    },
    showButtons () {
      this.$emit('show-operation-buttons')
    }
  },
  mounted () {
    this.isFixed && this.computedFxiedArea()
  }
}
</script>

<style lang="scss" scoped>
  @import '~@/assets/css/templateNormal.scss';

  .is-start-message {
    padding-top: 18px !important;;
  }

  .naver-chat-item {
    display: flex;
    transition: none;
    padding: 6px 20px;

    &.notice {
      padding: 10px 20px;
    }

    .profile {
      display: block;
      width: 30px;
      height: 30px;
      margin-right: 8px;
    }

    .profile-img {
      width: 100%;
      height: 100%;
      border-radius: 50%;
    }

    .chat-content {
      flex: 1;
      width: calc(100% - 38px);
    }

    .icon-badge {
      padding: 5px;
      display: inline-block;
      font-size: 9px;
      font-weight: bold;
      height: 16px;
      color: white;
      line-height: 7px;
      margin-right: 4px;
      border-radius: 2px;
      background: linear-gradient(to left, #f662a5, #7026fd);

      &.badge-gray {
        background: #999999;
      }
    }

    .separate-dot {
      display: inline-block;
      width: 2px;
      height: 2px;
      margin: 0 5px;
      border-radius: 50%;
      background-color: currentColor;
      vertical-align: middle;
    }

    .chat-sup {
      height: 14px;
      margin-top: 5px;
      color: #999999;

      .publish-time {
        display: inline-block;
        height: 100%;
        line-height: 14px;
      }

      .btn-report {
        height: 100%;
        line-height: 100%;
        font-size: 12px;
        color: inherit;
        transition: color .2s;

        &:hover {
          color: #ffffff;
        }

        &:active {
          color: #1e1e1f;
        }
      }
    }

    .message-emoji {
      display: block;
      width: 53px;
      height: 44px;
      margin: 5px 0;
    }

    .level {
      flex-shrink: 0;
    }

    .btn-retry {
      width: 16px;
      height: 16px;
      background: url(~@/assets/img/celeb-chat-retry-normal.svg) no-repeat;
      background-size: 100%;

      &:hover {
        background-image: url(~@/assets/img/celeb-chat-retry-over.svg);
        background-size: 100%;
      }

      &:active {
        background-image: url(~@/assets/img/celeb-chat-retry-clicked.svg);
        background-size: 100%;
      }
    }

    .btn-send-error {
      width: 16px;
      height: 16px;
      background: url(~@/assets/img/celeb-chat-error-normal.svg) no-repeat;
      background-size: 100%;

      &:hover {
        background-image: url(~@/assets/img/celeb-chat-error-over.svg);
        background-size: 100%;
      }

      &:active {
        background-image: url(~@/assets/img/celeb-chat-error-clicked.svg);
        background-size: 100%;
      }
    }
  }

  button[data-title] {
    display: flex;
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

  span.message-content.notice-message-content {
    font-size: 14px;
    font-weight: bold;
  }

  span.message-content.notice-message-content.fixed-message-text {
    color: #2d2d2d;
  }

  span.notice-green-color {
    color: #3df18c !important;
  }

  span.notice-green-yellow {
    color: #ffd000 !important;
  }

  span.notice-green-blue {
    color: #1cdbf8 !important;
  }

  .shopping-item {
    margin-bottom: 3px;
  }

  .message-box {
    font-size: 14px;
    letter-spacing: 1px;

    .message-content {
      font-weight: lighter;
      word-break: break-word;
    }

    &.start-message {
      .message-content {
        color: #bababa;
        font-weight: bold !important;
      }
    }
  }

  .fixed-notice-item {
    .message-box {
      width: 100%;
      text-overflow: -o-ellipsis-lastline;
      overflow: hidden;
      text-overflow: ellipsis;
      display: -webkit-box;
      -webkit-line-clamp: 2;
      -webkit-box-orient: vertical;
    }

    .notice-message-content {
      color: #3d3d3d;
      white-space: pre-wrap;
      text-overflow: -o-ellipsis-lastline;
      overflow: hidden;
      text-overflow: ellipsis;
      display: -webkit-box;
      -webkit-line-clamp: 2;
      -webkit-box-orient: vertical;
    }
  }

  .fixed-open-area {
    white-space: pre-line;
    word-break: break-all;
    position: absolute;
    left: 5px;
    width: calc(100% - 10px);
    top: 66px;
    padding: 10px;
    color: #bababa;
    z-index: 1000;
    opacity: 0.9;
    font-size: 12px;
    border-radius: 3px;
    border: solid 1px rgba(255, 255, 255, 0.25);
    background-color: black;
  }

  .fixed-message-temp {
    word-break: break-word;
    display: block;
    left: 0;
    position: absolute;
    visibility: hidden;
    font-size: 14px;
    padding: 20px;
  }
</style>
