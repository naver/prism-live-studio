const req = require.context('@/assets/img/colorBar/', false, /\.png$/)
const requireAll = requireContext => requireContext.keys().map(requireContext)
const list = requireAll(req)
export default list
