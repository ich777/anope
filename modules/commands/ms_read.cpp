/* MemoServ core functions
 *
 * (C) 2003-2013 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "module.h"

static ServiceReference<MemoServService> MemoServService("MemoServService", "MemoServ");

static void rsend_notify(CommandSource &source, MemoInfo *mi, Memo *m, const Anope::string &targ)
{
	/* Only send receipt if memos are allowed */
	if (MemoServService && !Anope::ReadOnly)
	{
		/* Get nick alias for sender */
		const NickAlias *na = NickAlias::Find(m->sender);

		if (!na)
			return;

		/* Get nick core for sender */
		const NickCore *nc = na->nc;

		if (!nc)
			return;

		/* Text of the memo varies if the recepient was a
		   nick or channel */
		Anope::string text = Anope::printf(Language::Translate(na->nc, _("\002[auto-memo]\002 The memo you sent to %s has been viewed.")), targ.c_str());

		/* Send notification */
		MemoServService->Send(source.GetNick(), m->sender, text, true);

		/* Notify recepient of the memo that a notification has
		   been sent to the sender */
		source.Reply(_("A notification memo has been sent to %s informing him/her you have\n"
				"read his/her memo."), nc->display.c_str());
	}

	/* Remove receipt flag from the original memo */
	m->receipt = false;
}

class MemoListCallback : public NumberList
{
	CommandSource &source;
	MemoInfo *mi;
	const ChannelInfo *ci;
 public:
	MemoListCallback(CommandSource &_source, MemoInfo *_mi, const ChannelInfo *_ci, const Anope::string &numlist) : NumberList(numlist, false), source(_source), mi(_mi), ci(_ci)
	{
	}

	void HandleNumber(unsigned number) anope_override
	{
		if (!number || number > mi->memos->size())
			return;

		MemoListCallback::DoRead(source, mi, ci, number - 1);
	}

	static void DoRead(CommandSource &source, MemoInfo *mi, const ChannelInfo *ci, unsigned index)
	{
		Memo *m = mi->GetMemo(index);
		if (!m)
			return;
			
		if (ci)
			source.Reply(_("Memo %d from %s (%s)."), index + 1, m->sender.c_str(), Anope::strftime(m->time).c_str());
		else
			source.Reply(_("Memo %d from %s (%s)."), index + 1, m->sender.c_str(), Anope::strftime(m->time).c_str());

		BotInfo *bi;
		Anope::string cmd;
		if (Command::FindCommandFromService("memoserv/del", bi, cmd))
		{
			if (ci)
				source.Reply(_("To delete, type: \002%s%s %s %s %d\002"), Config->StrictPrivmsg.c_str(), bi->nick.c_str(), cmd.c_str(), ci->name.c_str(), index + 1);
			else
				source.Reply(_("To delete, type: \002%s%s %s %d\002"), Config->StrictPrivmsg.c_str(), bi->nick.c_str(), cmd.c_str(), index + 1);
		}

		source.Reply("%s", m->text.c_str());
		m->unread = false;

		/* Check if a receipt notification was requested */
		if (m->receipt)
			rsend_notify(source, mi, m, ci ? ci->name : source.GetNick());
	}
};

class CommandMSRead : public Command
{
 public:
	CommandMSRead(Module *creator) : Command(creator, "memoserv/read", 1, 2)
	{
		this->SetDesc(_("Read a memo or memos"));
		this->SetSyntax(_("[\037channel\037] {\037num\037 | \037list\037 | LAST | NEW}"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) anope_override
	{

		MemoInfo *mi;
		ChannelInfo *ci = NULL;
		Anope::string numstr = params[0], chan;

		if (!numstr.empty() && numstr[0] == '#')
		{
			chan = numstr;
			numstr = params.size() > 1 ? params[1] : "";

			ci = ChannelInfo::Find(chan);
			if (!ci)
			{
				source.Reply(CHAN_X_NOT_REGISTERED, chan.c_str());
				return;
			}
			else if (!source.AccessFor(ci).HasPriv("MEMO"))
			{
				source.Reply(ACCESS_DENIED);
				return;
			}
			mi = &ci->memos;
		}
		else
			mi = const_cast<MemoInfo *>(&source.nc->memos);

		if (numstr.empty() || (!numstr.equals_ci("LAST") && !numstr.equals_ci("NEW") && !numstr.is_number_only()))
			this->OnSyntaxError(source, numstr);
		else if (mi->memos->empty())
		{
			if (!chan.empty())
				source.Reply(MEMO_X_HAS_NO_MEMOS, chan.c_str());
			else
				source.Reply(MEMO_HAVE_NO_MEMOS);
		}
		else
		{
			int i, end;

			if (numstr.equals_ci("NEW"))
			{
				int readcount = 0;
				for (i = 0, end = mi->memos->size(); i < end; ++i)
					if (mi->GetMemo(i)->unread)
					{
						MemoListCallback::DoRead(source, mi, ci, i);
						++readcount;
					}
				if (!readcount)
				{
					if (!chan.empty())
						source.Reply(MEMO_X_HAS_NO_NEW_MEMOS, chan.c_str());
					else
						source.Reply(MEMO_HAVE_NO_NEW_MEMOS);
				}
			}
			else if (numstr.equals_ci("LAST"))
			{
				for (i = 0, end = mi->memos->size() - 1; i < end; ++i);
				MemoListCallback::DoRead(source, mi, ci, i);
			}
			else /* number[s] */
			{
				MemoListCallback list(source, mi, ci, numstr);
				list.Process();
			}
		}
		return;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand) anope_override
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Sends you the text of the memos specified. If LAST is\n"
				"given, sends you the memo you most recently received. If\n"
				"NEW is given, sends you all of your new memos.  Otherwise,\n"
				"sends you memo number \037num\037. You can also give a list of\n"
				"numbers, as in this example:\n"
				" \n"
				"   \002READ 2-5,7-9\002\n"
				"      Displays memos numbered 2 through 5 and 7 through 9."));
		return true;
	}
};

class MSRead : public Module
{
	CommandMSRead commandmsread;

 public:
	MSRead(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, VENDOR),
		commandmsread(this)
	{

	}
};

MODULE_INIT(MSRead)
