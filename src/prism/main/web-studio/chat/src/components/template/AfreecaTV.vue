<template>
  <div class="chat-info-item afreecatv-chat-item" :class="{'is-self': isSelf, 'is-manager': isManager}">
    <template v-if="data.rawMessage.message">
      <div class="title">
        <div class="channel-img" v-if="!noChannelImage">
          <i class="icon-channel" :class="[data.livePlatform]"></i>
        </div>
        <div v-if="data._badges && data._badges.length" class="badge-img-box">
          <img v-for="(img, index) in data._badges" :key="index" class="badge-img" :src="img" alt="">
        </div>
        <div class="author-name">{{data._displayName}}</div>
      </div>
      <div class="message" v-html="data._message"></div>
      <div class="chat-sup">{{data.timeago}}</div>
    </template>
    <div v-else class="notice">{{data._notice}}</div>
  </div>
</template>

<script>
export default {
  name: 'templateAfreecaTV',
  props: {
    data: {
      type: Object,
      default() {
        return {}
      }
    },
    noChannelImage: Boolean
  },
  computed: {
    isSelf () {
      return this.data._isSelf && this.data.rawMessage.message
    },
    isManager () {
      return this.data._isManager && this.data.rawMessage.message
    }
  }
}
</script>

<style lang="scss">
  @import '~@/assets/css/templateNormal.scss';

  .afreecatv-chat-item {
    .notice {
      color: #bababa;
      font-size: 14px;
      font-weight: bold;
    }
    .afreecatv-emoticon{
      height: 25px;
    }
  }
</style>
