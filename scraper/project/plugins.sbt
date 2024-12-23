// Eclipse
addSbtPlugin("com.typesafe.sbteclipse" % "sbteclipse-plugin" % "4.0.0")

// The Play plugin
addSbtPlugin("com.typesafe.play" % "sbt-plugin" % "2.5.4")

// web plugins

addSbtPlugin("com.typesafe.sbt" % "sbt-jshint" % "1.0.3")

addSbtPlugin("com.typesafe.sbt" % "sbt-digest" % "1.1.0")

addSbtPlugin("com.github.ddispaltro" % "sbt-reactjs" % "0.5.2")

lazy val root = (project in file(".")).dependsOn(concatPlugin)
lazy val concatPlugin = uri("https://github.com/ground5hark/sbt-concat.git#342acc34195438799b8a278fda94b126238aae17")
