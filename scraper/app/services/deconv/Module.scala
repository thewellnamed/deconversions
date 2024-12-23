package services.deconv

import play.libs.akka.AkkaGuiceSupport
import com.google.inject.AbstractModule

class Module extends AbstractModule with AkkaGuiceSupport {
  def configure() = {
    bindActor(classOf[DeconvService], "deconv-service")
    bind(classOf[DeconvDAO]).asEagerSingleton()
  }
}