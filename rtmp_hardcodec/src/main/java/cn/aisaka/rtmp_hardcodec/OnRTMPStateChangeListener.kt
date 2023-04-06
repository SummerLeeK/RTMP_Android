package cn.aisaka.rtmp_hardcodec

interface OnRTMPStateChangeListener {

    fun onCreate()

    fun onConnect()

    fun onDisconnect()

    fun onMessageReceive()

    fun onPause()

    fun onResume()

    fun onStop()

    fun onRelease()
}