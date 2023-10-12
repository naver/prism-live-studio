<template>
  <transition name="fade">
    <div ref="toast" class="toast-rtmp" v-if="show" :style="position">
      <div class="toast-rtmp-container">
        <btn-close class="toast-btn-close" @click="handleToastClose" is-small></btn-close>
        <span v-html="content"></span>
      </div>
    </div>
  </transition>
</template>

<script>
import BtnClose from '@/components/BtnClose.vue'
export default {
  name: 'prismToast',
  components: { BtnClose },
  data() {
    return {
      show: false,
      position: '',
      content: ''
    }
  },
  methods: {
    handleToastClose() {
      this.show = false
    }
  },
  mounted() {
    this.show = true
    setTimeout(() => {
      this.handleToastClose()
    }, 5000)
  }
}
</script>

<style lang="scss">
  .toast-rtmp {
    position: absolute;
    width: 100%;
    padding: 0 10px;

    .toast-rtmp-container {
      position: relative;
      padding: 10px 22px;
      border-radius: 3px;
      border: 1px solid rgba(255, 255, 255, 0.1);
      text-align: center;
      color: #bababa;
      background: #1e1e1f;
    }
    .toast-btn-close {
      position: absolute;
      top: 4px;
      right: 4px;
    }
  }
  .fade-leave-active {
    animation: fade-out 1s forwards;
  }
  @keyframes fade-out {
    0% {
      opacity: 1;
      max-height: 200px;
    }
    50% {
      opacity: 0;
      max-height: 200px;
    }
    100% {
      opacity: 0;
      max-height: 0;
    }
  }
</style>
