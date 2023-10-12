<template>
  <div class="chat-info-item" :class="{'is-self': data._isSelf, 'is-manager': !data._isSelf && data._isManager}">
    <div class="title">
      <div class="channel-img" v-if="!noChannelImage">
        <i class="icon-channel" :class="[data.livePlatform]"></i>
      </div>
      <div v-if="badges" class="badge-img-box medium">
        <img v-for="(img, index) in badges" :key="index" class="badge-img" :src="img" alt="">
      </div>
      <div class="author-name">{{data.author.displayName}}</div>
    </div>
    <twitch-message v-if="isTwitch" :data="data"></twitch-message>
    <FacebookMessage v-else-if="isFacebook" :data="data"></FacebookMessage>
    <YoutubeMessage v-else-if="isYoutube" :data="data"></YoutubeMessage>
    <div v-else class="message">{{data.message}}</div>
    <div class="chat-sup">{{data.timeago}}</div>
    <div class="chat-btns">
      <btn-close
        v-if="data._isSelf && isPlatformChatCanDeleted(data.livePlatform)"
        class="chat-btn-close"
        is-small
        @click="handleDelete"></btn-close>
    </div>
  </div>
</template>

<script>
import { CHAT_CAN_DELETED_PLATFORMS, CHANNEL } from '@/assets/js/constants'
import { TWITCH_BADGES_LIBRARY } from '@/assets/js/twitchResources'
import BtnClose from '@/components/BtnClose.vue'
import TwitchMessage from './TwitchMessage.vue'
import FacebookMessage from './FacebookMessage.vue'
import YoutubeMessage from './YoutubeMessage.vue'

export default {
  name: 'templateNormal',
  components: {BtnClose, TwitchMessage, FacebookMessage, YoutubeMessage},
  props: {
    data: {
      type: Object,
      default () {
        return {}
      }
    },
    noChannelImage: Boolean
  },
  data () {
    return {
      library: TWITCH_BADGES_LIBRARY
    }
  },
  computed: {
    isTwitch () {
      return this.data.livePlatform === CHANNEL.TWITCH
    },
    isFacebook () {
      return this.data.livePlatform === CHANNEL.FACEBOOK
    },
    isYoutube () {
      return this.data.livePlatform === CHANNEL.YOUTUBE
    },
    badges () {
      let res = null
      if (this.isTwitch) {
        const badges = this.data.rawMessage.badges
        if (badges && this.library.total) {
          res = []
          badges.split(',').forEach(el => {
            const [name, version] = el.split('/')
            try {
              res.push(this.library.channel[name].versions[version].image_url_4x)
            } catch {
              try {
                res.push(this.library.total[name].versions[version].image_url_4x)
              } catch {
                console.error('can not find the picture of badge: ', name, ' (version ', version, ')')
              }
            }
          })
        }
      }
      return res
    }
  },
  methods: {
    isPlatformChatCanDeleted (platformName) {
      return CHAT_CAN_DELETED_PLATFORMS.includes(platformName)
    },
    handleDelete () {
      this.$emit('handle-delete', this.data)
    }
  }
}
</script>

<style lang="scss">
  @import '~@/assets/css/templateNormal.scss';

  .icon-channel {
    display: block;
    width: 100%;
    height: 100%;
    background-repeat: no-repeat;
    background-size: 100% 100%;

    &.FACEBOOK {
      background-image: url(~@/assets/img/facebook.svg);
    }

    &.YOUTUBE {
      background-image: url(~@/assets/img/youtube.svg);
    }

    &.TWITCH {
      background-image: url(~@/assets/img/twitch.svg);
    }

    &.NAVERTV {
      background-image: url(~@/assets/img/navertv.svg);
    }

    &.AFREECATV {
      background-image: url(~@/assets/img/afreecatv.svg);
    }
  }
</style>
