package services.imgur

import play.libs.akka.AkkaGuiceSupport
import com.google.inject.AbstractModule

class Module extends AbstractModule with AkkaGuiceSupport {
  def configure() = {
    bindActor(classOf[ImgurService], "imgur-service")
    bind(classOf[ImgurDAO]).asEagerSingleton()
  }
}