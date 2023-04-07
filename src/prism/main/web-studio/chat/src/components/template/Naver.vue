<template>
  <div class="chat-info-item naver-chat-item" :class="{'is-self': data._isSelf,'is-manager':data._isManager}">
    <div class="chat-content">
      <div class="title">
        <div v-if="isVlive&&data.extension&&data.extension.bdg" class="icon-badge" :class="{'badge-gray':+data.extension.t !== 0}">{{ data.extension.bdg }}</div>
        <div class="author-name">
          <span>{{ data.userName }}</span>
        </div>
      </div>
      <div class="message">
        <img v-if="data.stickerId" class="message-emoji" :src="getChatImageUrl" alt="">
        <span v-html="data.contents"></span>
      </div>
      <div class="chat-sup">
        <span class="publish-time">{{ data.timeago }}</span>
        <template v-if="!data._isSelf&&!data._isManager">
          <i class="separate-dot"></i>
          <button class="btn-report" @click="handleReport">{{ $i18n.report }}</button>
        </template>
      </div>
      <div v-if="data._isSelf" class="chat-btns">
        <template v-if="data._sendError">
          <div v-if="data._isShowErrorHandleBtns">
            <button class="btn-retry" @click="handleRetry" :data-title="$i18n.tooltip.retry"></button>
            <btn-close is-small @click="handleDelete(true)" :data-title="$i18n.tooltip.deleteText"></btn-close>
          </div>
          <button v-else class="btn-send-error" :data-title="$i18n.tooltip.sendError" @click="showButtons"></button>
        </template>
        <btn-close v-else class="chat-btn-close" is-small @click="handleDelete" :data-title="$i18n.tooltip.deleteText"></btn-close>
      </div>
    </div>
  </div>
</template>

<script>
import { VLIVE_CHAT_IMAGE_BASE_URL, VLIVE_CHAT_IMAGE_BASE_URL_PRO } from '@/assets/js/constants'
import BtnClose from '@/components/BtnClose.vue'

export default {
  name: 'templateNaver',
  components: {BtnClose},
  props: {
    data: {
      type: Object,
      default () {
        return {}
      }
    },
    isDev: Boolean,
    isVlive: Boolean
  },
  computed: {
    getChatImageUrl () {
      return `${this.isDev ? VLIVE_CHAT_IMAGE_BASE_URL : VLIVE_CHAT_IMAGE_BASE_URL_PRO}${this.data.stickerId}.png`
    }
  },
  methods: {
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
  }
}
</script>

<style lang="scss">
@import '~@/assets/css/templateNormal.scss';

.naver-chat-item.is-manager {
  background-color: #ffffff;

  .author-name {
    font-weight: bold;
    color: #000;

    span {
      font-weight: 700;
    }
  }

  .message {
    color: #666666;
  }

  .chat-sup {
    color: #bbbbbb;
  }
}

.naver-chat-item {
  display: flex;
  transition: none;

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
      background: #6A6A7B;
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
    }

    &:active {
      background-image: url(~@/assets/img/celeb-chat-retry-clicked.svg);
    }
  }

  .btn-send-error {
    width: 16px;
    height: 16px;
    background: url(~@/assets/img/celeb-chat-error-normal.svg) no-repeat;
    background-size: 100%;

    &:hover {
      background-image: url(~@/assets/img/celeb-chat-error-over.svg);
    }

    &:active {
      background-image: url(~@/assets/img/celeb-chat-error-clicked.svg);
    }
  }
}

button[data-title]:hover:after {
  content: attr(data-title);
  position: absolute;
  width: max-content;
  top: 18px;
  right: 18px;
  padding: 2px 4px;
  border: 1px solid #000000;
  border-radius: 1px;
  background-color: #111111;
  color: #ffffff;
}
</style>
