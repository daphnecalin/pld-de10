using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.RazorPages;

namespace QuartusDebugger.Web.Pages;

public sealed class DetailsModel(SubmissionStore store) : PageModel
{
    public Submission Item { get; private set; } = null!;
    public IEnumerable<string> Downloads => Item.AvailableFiles();
    public async Task<IActionResult> OnGetAsync(string id)
    {
        var item = await store.FindAsync(id, User, HttpContext.RequestAborted);
        if (item == null) return NotFound();
        Item = item;
        return Page();
    }
}
