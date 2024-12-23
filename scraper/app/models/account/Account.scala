package models.account

import com.mohiva.play.silhouette.api.{ Authorization, Identity, LoginInfo, AuthInfo }
import com.mohiva.play.silhouette.api.util.PasswordInfo
import com.mohiva.play.silhouette.impl.authenticators.JWTAuthenticator
import java.time.Instant
import play.api.libs.json._
import play.api.libs.json.Json._
import play.api.data._
import play.api.mvc.Request
import scala.concurrent.Future
        
case class PasswordReset (password: String, authInfo: PasswordInfo)

case class Account (
    id: Int, 
    email: String,
    name: String,
    perms: Int) extends Identity

case class CreateAccount(registration: Form[RegistrationForm.Data], perms: Int)
    
case class WithPermission(perms: Int) extends Authorization[Account, JWTAuthenticator] {
  def isAuthorized[B](acct: Account, authenticator: JWTAuthenticator)(implicit request: Request[B]) = {
    Future.successful((acct.perms & perms) == perms)
  }
}

object AccountPermission {
  val View = 0x01
  val Edit = 0x02
  val Admin = 0x04
}

object AccountType {
  val Guest = AccountPermission.View
  val Coder = AccountPermission.View | AccountPermission.Edit
  val Admin = AccountPermission.View | AccountPermission.Edit | AccountPermission.Admin
}