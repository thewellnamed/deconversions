package models.account

import play.api.data.Form
import play.api.data.Forms._

/**
 * The form which handles the sign up process.
 */
object RegistrationForm {
  val form = Form(
    mapping(
      "email"     -> nonEmptyText,
      "name"      -> nonEmptyText,
      "password"  -> nonEmptyText,
      "confirmPassword" -> nonEmptyText
    )(Data.apply)(Data.unapply) verifying(
      "error.confpassmatch", fields => 
        fields match {
          case data => validatePassword(data).isDefined
        }
    )
  )
  
  def validatePassword(data: Data) = {
    (data.password == data.confirmPassword) match {
      case true => Some(data)
      case false => None
    }
  }
  
  case class Data(
    email: String,
    name: String,
    password: String,
    confirmPassword: String)
}