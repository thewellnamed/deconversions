package models

import play.api.mvc.QueryStringBindable
import play.api.libs.json._
import play.api.libs.json.Json._
import scala.Left
import scala.Right
import scala.math.BigDecimal.int2bigDecimal

object CodeValue extends Enumeration {
  type CodeValue = Value
  val None = Value(0)
  
  // LAST_VALUE = 36
  
  // Men as Victims
  val MenAsVictims = Value(5)
  val MenAsVictimsSexism = Value(24)
  val MenAsVictimsRape = Value(25)
  val MenAsVictimsDomesticViolence = Value(26)
  val MenAsVictimsDisease = Value(27)
  val MenAsVictimsSuicide = Value(28)
  val MenAsVictimsGenderRoles = Value(1)
  val MenAsVictimsLegalSystem = Value(29)
  val MenAsVictimsHarassment = Value(34)
  
  // Denial of Inequality
  val DenialInequality = Value(4)
  val DenialWageGap = Value(30)
  val DenialSexismAgainstWomen = Value(31)
  val DenialRape = Value(2)
  val DenialDomesticViolence = Value(3)
  val WomenMorePrivileged = Value(22)
  val WomenFalseAccusations = Value(19)
  
  // Anti-Feminism
  val AssertionOfHypocrisy = Value(18)
  val AccusationOfMisandry = Value(32)
  val MisrepresentationFeminism = Value(6)
  val BacklashAgainstFeminism = Value(7)
  val MisappropriatingFeminism = Value(20)
  val FallacyDramaticInstance = Value(8)
  val ReassertionOfPatriarchy = Value(15)
  val FalseEquivalence = Value(14)
  val PostFeminism = Value(16)
  
  // Pro-Men
  val MaleSocialization = Value(17)
  val RecognitionMensProblems = Value(23)
  val CallToAction = Value(21)
  
  // No meta-code
  val TyrannySnuggle = Value(9)
  val ChildSupport = Value(10)
  val ChildCustody = Value(11)
  val IntersectionalityClass = Value(12)
  val IntersectionalityRace = Value(13)
  val MisrepresentingStats = Value(33)
  val Abortion = Value(35)
  val Circumcision = Value(36)
   
  implicit val jsonWrite: Writes[CodeValue] = new Writes[CodeValue] {
    def writes(s: CodeValue): JsValue = JsNumber(s.id)
  }
  
  implicit val jsonRead: Reads[CodeValue] = new Reads[CodeValue] {
    def reads(in: JsValue): JsResult[CodeValue] = {
      in match {
        case JsString(s) => {
          try {
            JsSuccess(CodeValue(s.toInt))
          } catch {
            case _: NoSuchElementException => JsError(s"Cannot map $s to AuthProviderType")
          }
        }
        
        case JsNumber(n) => {
          try {
            JsSuccess(CodeValue(n.toInt))
          }
          catch {
            case _: NoSuchElementException => JsError(s"Cannot map $n to AuthProviderType")
          }
        }
        
        case _ => JsError("cannot map unknown input to AuthProviderType")
      }
    }
  }  
  
  implicit def bindableAuthProviderType = new QueryStringBindable[CodeValue] {
    def bind(key: String, params: Map[String, Seq[String]]) = {
      params.get(key).flatMap(_.headOption).map { p =>
        CodeValue(p.toInt) match {
          case s: CodeValue => Right(s)
          case _ => Left(s"Cannot parse parameter $key as an Enum: AuthProviderType")
        }
      }
    }
    def unbind(key: String, value: CodeValue) = value.toString
  }
}