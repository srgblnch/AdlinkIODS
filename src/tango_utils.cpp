#include "tango_utils.h"

std::ostream & operator<<(std::ostream & os, const CORBA::Exception &e)
{
	print_exception(os, e);
	return os;
}



//+----------------------------------------------------------------------------
//
// method : 		print_exception
//
// description : 	This method prints the information embedded in
//			a Tango exception.
//
// in :			e : Reference to the exception object
//
//-----------------------------------------------------------------------------

void print_exception(std::ostream & stream, const CORBA::Exception &e)
{

	const CORBA::SystemException *se;
	const CORBA::UserException *ue;

//
// For a CORBA::SystemException, use the OB function
//

	if ((se = dynamic_cast<const CORBA::SystemException *>(&e)) != NULL)
	{
		stream << Tango::Except::print_CORBA_SystemException(se) << endl;
	}

//
// If it is a CORBA::UserException
//

	else if ((ue = dynamic_cast<const CORBA::UserException *>(&e)) != NULL)
	{
		const Tango::DevFailed *te;
		const Tango::NamedDevFailedList *mdf;

//
// Print the Tango::NamedDevFailedList exception contents
//

		if ((mdf = dynamic_cast<const Tango::NamedDevFailedList *>(&e)) != NULL)
		{
			stream << "Tango NamedDevFailedList exception" << endl;
			for (unsigned long i = 0;i < mdf->err_list.size();i++)
			{
				stream << "   Exception for object " << mdf->err_list[i].name << endl;
				stream << "   Index of object in call (starting at 0) = " << mdf->err_list[i].idx_in_call << endl;
				for (unsigned long j =0;j < mdf->err_list[i].err_stack.length();j++)
				{
					stream << "       Severity = ";
					switch (mdf->err_list[i].err_stack[j].severity)
					{
						case Tango::WARN :
							stream << "WARNING ";
							break;

						case Tango::ERR :
							stream << "ERROR ";
							break;

						case Tango::PANIC :
							stream << "PANIC ";
							break;

						default :
							stream << "Unknown severity code";
							break;
					}
					stream << endl;
					stream << "       Error reason = " << mdf->err_list[i].err_stack[j].reason.in() << endl;
					stream << "       Desc : " << mdf->err_list[i].err_stack[j].desc.in() << endl;
					stream << "       Origin : " << mdf->err_list[i].err_stack[j].origin.in() << endl;
				}
			}
			stream << "   Summary exception" << endl;
			for (unsigned long j =0;j < mdf->errors.length();j++)
			{
				stream << "       Severity = ";
				switch (mdf->errors[j].severity)
				{
					case Tango::WARN :
						stream << "WARNING ";
						break;

					case Tango::ERR :
						stream << "ERROR ";
						break;

					case Tango::PANIC :
						stream << "PANIC ";
						break;

					default :
						stream << "Unknown severity code";
						break;
				}
				stream << endl;
				stream << "       Error reason = " << mdf->errors[j].reason.in() << endl;
				stream << "       Desc : " << mdf->errors[j].desc.in() << endl;
				stream << "       Origin : " << mdf->errors[j].origin.in() << endl;
				stream << endl;
			}

		}

//
// Print the Tango::DevFailed exception contents
//

		else if ((te = dynamic_cast<const Tango::DevFailed *>(&e)) != NULL)
		{
			for (unsigned long i =0;i < te->errors.length();i++)
			{
				stream << "Tango exception" << endl;
				stream << "Severity = ";
				switch (te->errors[i].severity)
				{
					case Tango::WARN :
						stream << "WARNING ";
						break;

					case Tango::ERR :
						stream << "ERROR ";
						break;

					case Tango::PANIC :
						stream << "PANIC ";
						break;

					default :
						stream << "Unknown severity code";
						break;
				}
				stream << endl;
				stream << "Error reason = " << te->errors[i].reason.in() << endl;
				stream << "Desc : " << te->errors[i].desc.in() << endl;
				stream << "Origin : " << te->errors[i].origin.in() << endl;
				stream << endl;
			}
		}

//
// For an unknown CORBA::UserException
//

		else
		{
			stream << "Unknown CORBA user exception" << endl;
		}
	}

//
// For an unknown exception type
//

	else
	{
		stream << "Unknown exception type !!!!!!" << endl;
	}

}



