using Microsoft.AspNetCore.Identity;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.RazorPages;
using Microsoft.AspNetCore.RateLimiting;

namespace QuartusDebugger.Web.Pages;

[EnableRateLimiting("uploads")]
public sealed class IndexModel(SubmissionStore store, UserManager<StudentAccount> users) : PageModel
{
    [BindProperty] public IFormFile? ProjectZip { get; set; }
    public bool All { get; private set; }
    public string AccountName { get; private set; } = "";
    public IReadOnlyList<Submission> Items { get; private set; } = [];

    public async Task<IActionResult> OnGetAsync(bool all = false)
    {
        if (all && !User.IsInRole("TA")) return Forbid();
        All = all;
        var user = await users.GetUserAsync(User);
        AccountName = user == null ? "" : $"{user.FirstName} {user.LastName}";
        Items = store.List(User, all).ToList();
        return Page();
    }

    public async Task<IActionResult> OnPostAsync()
    {
        var user = await users.GetUserAsync(User);
        if (user == null) return Challenge();
        if (ProjectZip == null) ModelState.AddModelError("", "Choose a project ZIP.");
        else
        {
            try
            {
                var item = await store.UploadAsync(ProjectZip, user, HttpContext.RequestAborted);
                return RedirectToPage("/Details", new { id = item.Id });
            }
            catch (InvalidDataException error) { ModelState.AddModelError("", error.Message); }
        }
        return await OnGetAsync();
    }
}
