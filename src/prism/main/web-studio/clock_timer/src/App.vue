<template>
  <div id="app">
    <TestDesk v-if="isDev"/>
    <Viewer v-else/>
  </div>
</template>

<script>
import TestDesk from './components/TestDesk.vue'
import Viewer from './components/Viewer.vue'
export default {
  name: 'App',
  components: {
    TestDesk, Viewer
  }, data () {
    return {
      isDev: new URLSearchParams(document.location.search).get('debugger') == '1'
    }
  }
}
</script>

<style lang="scss">
  * {
    margin: 0;
    padding: 0;
  }

  html, body {
    height: 100%;
    width: 100%;
    overflow: hidden;
  }

  #app {
    height: 100%;
    width: 100%;
    display: flex;
    align-items: center;
    justify-content: center;
  }
</style>
