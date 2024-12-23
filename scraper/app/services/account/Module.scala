package services.account

import com.google.inject.name.Named
import com.google.inject.{ AbstractModule, Provides }
import com.mohiva.play.silhouette.api.{ AuthInfo, Env, Environment, EventBus, Silhouette, SilhouetteProvider }
import com.mohiva.play.silhouette.api.crypto.{ CookieSigner, Crypter, CrypterAuthenticatorEncoder }
import com.mohiva.play.silhouette.crypto.{ JcaCookieSigner, JcaCookieSignerSettings, JcaCrypter, JcaCrypterSettings }
import com.mohiva.play.silhouette.api.repositories.{ AuthenticatorRepository, AuthInfoRepository }
import com.mohiva.play.silhouette.api.services._
import com.mohiva.play.silhouette.api.util._
import com.mohiva.play.silhouette.impl.authenticators._
import com.mohiva.play.silhouette.impl.providers._
import com.mohiva.play.silhouette.impl.services._
import com.mohiva.play.silhouette.impl.util._
import com.mohiva.play.silhouette.password.BCryptPasswordHasher
import com.mohiva.play.silhouette.persistence.daos.DelegableAuthInfoDAO
import com.mohiva.play.silhouette.persistence.repositories.DelegableAuthInfoRepository
import net.codingwell.scalaguice.ScalaModule
import net.ceedubs.ficus.Ficus._
import net.ceedubs.ficus.readers.ArbitraryTypeReader._
import net.ceedubs.ficus.readers.EnumerationReader._
import play.api.Configuration
import play.api.libs.concurrent.Execution.Implicits._
import play.api.libs.ws.WSClient

import models.account._

trait AccountEnv extends Env {
  type I = Account
  type A = JWTAuthenticator
}

class Module extends AbstractModule with ScalaModule {
  def configure() {
    bind[Silhouette[AccountEnv]].to[SilhouetteProvider[AccountEnv]]
    bind[EventBus].toInstance(EventBus())
    bind[PasswordHasher].toInstance(new BCryptPasswordHasher)
    bind[IDGenerator].toInstance(new SecureRandomIDGenerator())
    bind[Clock].toInstance(Clock())
    bind[DelegableAuthInfoDAO[PasswordInfo]].to[PasswordAuthInfoService]
    bind[AuthenticatorRepository[JWTAuthenticator]].to[SessionService]
    bind[CacheLayer].to[PlayCacheLayer]
    bind[AccountService].asEagerSingleton()
  }
  
  @Provides
  def provideHTTPLayer(client: WSClient): HTTPLayer = new PlayHTTPLayer(client)
  
  @Provides 
  def provideEnvironment(
    identityService: AccountService,
    authenticatorService: AuthenticatorService[JWTAuthenticator],
    eventBus: EventBus): Environment[AccountEnv] = {
      Environment[AccountEnv](identityService, authenticatorService, Seq(), eventBus)
  }
  
  @Provides @Named("authenticator-crypter")
  def provideAuthenticatorCrypter(configuration: Configuration): Crypter = {
    val config = configuration.underlying.as[JcaCrypterSettings]("silhouette.authenticator.crypter")

    new JcaCrypter(config)
  }
  
  @Provides
  def provideAuthInfoRepository(
    passwordAuthInfoService: DelegableAuthInfoDAO[PasswordInfo]): AuthInfoRepository = {
    new DelegableAuthInfoRepository(passwordAuthInfoService)
  }  
  
  @Provides
  def provideAuthenticatorService(
    authRepository: AuthenticatorRepository[JWTAuthenticator],
    @Named("authenticator-crypter") crypter: Crypter,
    idGenerator: IDGenerator,
    configuration: Configuration,
    clock: Clock): AuthenticatorService[JWTAuthenticator] = {

    val config = configuration.underlying.as[JWTAuthenticatorSettings]("silhouette.authenticator")
    val encoder = new CrypterAuthenticatorEncoder(crypter)

    new JWTAuthenticatorService(config, Some(authRepository), encoder, idGenerator, clock)
  }
    
  @Provides
  def provideCredentialsProvider(
    authInfoRepository: AuthInfoRepository,
    passwordHasherRegistry: PasswordHasherRegistry): CredentialsProvider = {

    new CredentialsProvider(authInfoRepository, passwordHasherRegistry)
  }
  
  @Provides
  def providePasswordHasherRegistry(passwordHasher: PasswordHasher): PasswordHasherRegistry = {
    new PasswordHasherRegistry(passwordHasher)
  }
}