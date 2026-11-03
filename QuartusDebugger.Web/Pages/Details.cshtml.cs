using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.RazorPages;

namespace QuartusDebugger.Web.Pages;

public sealed class DetailsModel(SubmissionStore store) : PageModel
{
    public Submission Item { get; private set; } = null!;
    public List<string> Downloads { get; } = [];
    public IActionResult OnGet(string id)
    {
        var item = store.Find(id, User);
        if (item == null) return NotFound();
        Item = item;
        foreach (string name in new[] { "source.zip", "debug_ip.sv", "debug_map.hpp" })
            if (store.DownloadPath(id, name, User) != null) Downloads.Add(name);
        return Page();
    }
}
