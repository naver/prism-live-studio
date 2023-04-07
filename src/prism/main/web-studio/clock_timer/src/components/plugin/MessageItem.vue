<template>
  <div :class="{play:isPlay}" class="message-item">
    <div :class="{current:isActive0,before:!isActive0}">{{ isActive0 ? current : before }}</div>
    <div :class="{current:!isActive0,before:isActive0}">{{ !isActive0 ? current : before }}</div>
  </div>
</template>

<script>
export default {
  name: 'MessageItem',
  props: {
    current: {
      type: Number,
      default: 0
    },
    styles: {
      type: Object,
      default () {
        return {
          color: '#ff4d4d',
          background: 'white'
        }
      }
    }
  },
  data () {
    return {
      before: 0,
      isActive0: true,
      isPlay: true
    }
  },
  watch: {
    current (current, preCurrent) {
      this.before = preCurrent
      this.isActive0 = !this.isActive0
    }
  }
}
</script>
<style lang="scss" scoped>
.message-item {
  position: relative;
  height: 28px;
  width: 16px;

  .current {
    position: absolute;
    opacity: 1;
    top: 0;
  }

  .before {
    top: -100%;
    opacity: 0;
    position: absolute;
  }
}

.play {
  .current {
    animation: slideIn .66s both cubic-bezier(.65, 0, .35, 1);
  }

  .before {
    animation: slideOut .66s both cubic-bezier(.65, 0, .35, 1);
  }
}

@keyframes slideIn {
  0% {
    top: 100%;
  }
  100% {
    top: 0;
  }
}

@keyframes slideOut {
  0% {
    top: 0;
    opacity: 1;
  }
  100% {
    opacity: 0;
    top: -100%;
  }
}
</style>
