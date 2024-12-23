name := """Deconversion-study"""

version := "1.0.0"

lazy val root = (project in file(".")).enablePlugins(PlayScala, SbtWeb)

scalaVersion := "2.11.8"

libraryDependencies ++= Seq(
  jdbc,
  cache,
  ws,
  filters,
  specs2 % Test,
  "org.webjars" %% "webjars-play" % "2.4.0-1",
  "org.webjars" % "react" % "0.14.3",
  "org.webjars" % "jquery" % "2.2.0",
  "org.webjars.bower" % "js-cookie" % "2.1.1",
  "org.webjars" % "historyjs" % "1.8.0",
  "org.webjars" % "bootstrap" % "3.3.6",
  "org.postgresql" % "postgresql" % "9.4.1207.jre7",
  "com.typesafe.play" %% "anorm" % "2.5.0",
  "com.sksamuel.scrimage" %% "scrimage-core" % "2.1.0",
  "com.typesafe.akka" %% "akka-contrib" % "2.3.13",
  "net.codingwell" %% "scala-guice" % "4.0.1",
  "com.iheart" %% "ficus" % "1.4.2",
  "com.mohiva" %% "play-silhouette" % "4.0.0-RC1",
  "com.mohiva" %% "play-silhouette-password-bcrypt" % "4.0.0",
  "com.mohiva" %% "play-silhouette-persistence" % "4.0.0",
  "com.mohiva" %% "play-silhouette-crypto-jca" % "4.0.0",
  "net.ruippeixotog" %% "scala-scraper" % "2.0.0"
)

resolvers += "scalaz-bintray" at "http://dl.bintray.com/scalaz/releases"
resolvers += "Kaliber Repository" at "https://jars.kaliber.io/artifactory/libs-release-local"
resolvers += Resolver.jcenterRepo

pipelineStages := Seq(digest)
pipelineStages in Assets := Seq(concat, digest)

// http://stackoverflow.com/questions/28514890/cannot-get-sbt-concat-to-bundle-styles-from-sbt-sass-or-sbt-less
Concat.groups := {
  // Determine the output names of the style files to bundle
  // This is really roundabout because sbt-concat only offers 2 ways of
  // specifying files, relative paths and using a PathFinder, and the
  // latter approach restricts itself to source files instead of output files.
  val sourceDir = (sourceDirectory in Assets).value
  val jsxFiles = ((sourceDir / "javascripts") ** "*.jsx").getPaths
  val jsxRelativePaths = jsxFiles.map(_.stripPrefix(sourceDir.getPath).stripPrefix("/"))
  val outputRelativePaths = jsxRelativePaths.map(_.stripSuffix(".jsx") + ".js")
  
  Seq("javascripts/viewer-complete.js" -> group(outputRelativePaths))
}

// Play provides two styles of routers, one expects its actions to be injected, the
// other, legacy style, accesses its actions statically.
routesGenerator := InjectedRoutesGenerator
